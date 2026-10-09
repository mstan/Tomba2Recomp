#include "mod_plugins.h"
#include "cpu_state.h"

#define TOMBA2_ACTOR_QUEUE_CAPACITY 128u
#define TOMBA2_ACTOR_QUEUE_STOCK_CAPACITY 24u
#define TOMBA2_ACTOR_QUEUE_STOCK_END 0x800F2410u
#define TOMBA2_ACTOR_QUEUE_HEAD 0x1F80013Cu
#define TOMBA2_ACTOR_QUEUE_COUNT 0x1F800144u

/* The type 2/9 queue fills from its end towards its beginning. Correcting
 * the camera axes exposed its original 24-pointer limit in playable village
 * captures. Keep the original producers and consumer, but give this queue
 * its own bounded CPU guest allocation. No packet or adjacent stock storage
 * is borrowed. Root integration calls this file's activation from the
 * existing widescreen activation; each plugin ID permits one activation. */
static uint32_t tomba2_actor_queue;
static uint32_t tomba2_actor_queue_peak;

static uint32_t tomba2_actor_queue_end(void) {
    return tomba2_actor_queue + TOMBA2_ACTOR_QUEUE_CAPACITY * 4u;
}

static int tomba2_actor_queue_owned(uint32_t head, uint32_t count) {
    return tomba2_actor_queue && count <= TOMBA2_ACTOR_QUEUE_CAPACITY &&
           head == tomba2_actor_queue_end() - count * 4u;
}

static void tomba2_actor_queue_sample(uint32_t count) {
    if (count > tomba2_actor_queue_peak) {
        psx_mod_counter_add("tomba2.widescreen.queue0_peak_count",
                            count - tomba2_actor_queue_peak);
        tomba2_actor_queue_peak = count;
    }
}

void tomba2_actor_queue_activate(void) {
    /* Activation replays when the mod plan changes. The allocator is
     * monotonic and its guest contents participate in save-state storage. */
    if (!tomba2_actor_queue)
        tomba2_actor_queue = psx_mod_alloc_guest_memory(
            TOMBA2_ACTOR_QUEUE_CAPACITY * 4u, 4u);
    if (!tomba2_actor_queue)
        psx_mod_counter_add("tomba2.widescreen.queue0_allocation_failed", 1);
}

static void tomba2_actor_queue_reset(CPUState *cpu, uint32_t address) {
    if (!cpu || !tomba2_actor_queue ||
        psx_mod_widescreen_view_x_margin() <= 0) return;

    if (cpu->gpr[2] != TOMBA2_ACTOR_QUEUE_STOCK_END ||
        cpu->gpr[3] != 0x1F800000u ||
        psx_mod_read_word(address) != 0xAC62013Cu) goto reject;

    switch (address & 0x1FFFFFFFu) {
    case 0x0003BB90u: {
        /* The consumer has saved the completed producer count/head in a1/a0
         * and cleared only the new producer count. Preserve those saved
         * values: the original following stores publish the completed list
         * to scratch +146/+140 before drawing it. A stock list captured just
         * before activation is also safe to consume during this transition. */
        const uint32_t count = cpu->gpr[5];
        const uint32_t head = cpu->gpr[4];
        const int owned = tomba2_actor_queue_owned(head, count);
        const int stock = count <= TOMBA2_ACTOR_QUEUE_STOCK_CAPACITY &&
            head == TOMBA2_ACTOR_QUEUE_STOCK_END - count * 4u;
        if ((!owned && !stock) ||
            psx_mod_read_byte(0x1F800136u) != 0 ||
            psx_mod_read_half(TOMBA2_ACTOR_QUEUE_COUNT) != 0 ||
            psx_mod_read_word(TOMBA2_ACTOR_QUEUE_HEAD) != head ||
            psx_mod_read_word(0x8003BB78u) != 0x94450144u ||
            psx_mod_read_word(0x8003BB80u) != 0xA4400144u ||
            psx_mod_read_word(0x8003BB88u) != 0x8C64013Cu ||
            psx_mod_read_word(0x8003BB8Cu) != 0x24422410u ||
            psx_mod_read_word(0x8003BB94u) != 0x3C021F80u ||
            psx_mod_read_word(0x8003BB98u) != 0xA4450146u ||
            psx_mod_read_word(0x8003BB9Cu) != 0x3C021F80u ||
            psx_mod_read_word(0x8003BBA0u) != 0xAC440140u) goto reject;
        tomba2_actor_queue_sample(count);
        if (owned && count > TOMBA2_ACTOR_QUEUE_STOCK_CAPACITY)
            psx_mod_counter_add("tomba2.widescreen.queue0_over_stock_frames", 1);
        psx_mod_counter_add("tomba2.widescreen.queue0_resets", 1);
        break;
    }
    case 0x00079BB4u:
        /* Initialization stores the same v0 to both producer +13C and
         * consumer +140, then zeroes both counts. Let those original stores
         * perform the complete reset; do not write scratch state ourselves. */
        if (psx_mod_read_word(0x80079BACu) != 0x3C02800Fu ||
            psx_mod_read_word(0x80079BB0u) != 0x24422410u ||
            psx_mod_read_word(0x80079BB8u) != 0x3C031F80u ||
            psx_mod_read_word(0x80079BBCu) != 0xAC620140u ||
            psx_mod_read_word(0x80079BC0u) != 0x3C021F80u ||
            psx_mod_read_word(0x80079BC4u) != 0xA4400146u ||
            psx_mod_read_word(0x80079BC8u) != 0x3C021F80u ||
            psx_mod_read_word(0x80079BCCu) != 0x3C031F80u ||
            psx_mod_read_word(0x80079BD0u) != 0xA4400144u) goto reject;
        psx_mod_counter_add("tomba2.widescreen.queue0_initializations", 1);
        break;
    default:
        return;
    }
    cpu->gpr[2] = tomba2_actor_queue_end();
    return;
reject:
    psx_mod_counter_add("tomba2.widescreen.queue0_reset_guard_reject", 1);
}

typedef struct Tomba2QueueProducer {
    uint32_t compare, count_base_reg, count_load, unsigned_count_load;
    uint32_t reject_branch, branch_delay;
    uint32_t head_load, head_store, actor_store, count_store;
    uint32_t actor_store_word, count_store_word;
} Tomba2QueueProducer;

static const Tomba2QueueProducer tomba2_actor_queue_producers[] = {
    {0x8007708Cu, 7u, 0x84E20144u, 0x94E50144u, 0x1040FFF5u, 0x3C031F80u,
     0x80077098u, 0x800770A4u, 0x800770ACu, 0x800770B4u,
     0xAC86FFFCu, 0xA4E20144u},
    {0x80077640u, 6u, 0x84C20144u, 0x94C50144u, 0x10400024u, 0x3C031F80u,
     0x8007764Cu, 0x80077658u, 0x80077660u, 0x80077668u,
     0xAC93FFFCu, 0xA4C20144u},
    {0x80077E88u, 6u, 0x84C20144u, 0x94C50144u, 0x10400009u, 0x00803821u,
     0x80077E98u, 0x80077EA4u, 0x80077EACu, 0x80077EB0u,
     0xAC87FFFCu, 0xA4C20144u},
};

static void tomba2_actor_queue_capacity(CPUState *cpu, uint32_t address) {
    if (!cpu || !tomba2_actor_queue ||
        psx_mod_widescreen_view_x_margin() <= 0) return;

    const uint32_t count = psx_mod_read_half(TOMBA2_ACTOR_QUEUE_COUNT);
    const uint32_t head = psx_mod_read_word(TOMBA2_ACTOR_QUEUE_HEAD);
    /* A failed/absent reset or a stock save-state list retains the original
     * 24-entry predicate. Raising a limit never authorizes stock RAM writes. */
    if (!tomba2_actor_queue_owned(head, count)) return;

    const Tomba2QueueProducer *site = 0;
    for (unsigned i = 0; i < sizeof tomba2_actor_queue_producers /
                                  sizeof tomba2_actor_queue_producers[0]; ++i)
        if ((address & 0x1FFFFFFFu) ==
            (tomba2_actor_queue_producers[i].compare & 0x1FFFFFFFu)) {
            site = &tomba2_actor_queue_producers[i];
            break;
        }
    if (!site) return;
    if (cpu->gpr[2] != count || cpu->gpr[site->count_base_reg] != 0x1F800000u ||
        psx_mod_read_word(site->compare) != 0x28420018u ||
        psx_mod_read_word(site->compare - 8u) != site->count_load ||
        psx_mod_read_word(site->compare - 4u) != site->unsigned_count_load ||
        psx_mod_read_word(site->compare + 4u) != site->reject_branch ||
        psx_mod_read_word(site->compare + 8u) != site->branch_delay ||
        psx_mod_read_word(site->head_load - 4u) != 0x3C031F80u ||
        psx_mod_read_word(site->head_load) != 0x8C64013Cu ||
        psx_mod_read_word(site->head_store - 4u) != 0x2482FFFCu ||
        psx_mod_read_word(site->head_store) != 0xAC62013Cu ||
        psx_mod_read_word(site->head_store + 4u) != 0x24A20001u ||
        psx_mod_read_word(site->actor_store) != site->actor_store_word ||
        psx_mod_read_word(site->count_store) != site->count_store_word) {
        psx_mod_counter_add("tomba2.widescreen.queue0_capacity_guard_reject", 1);
        return;
    }
    tomba2_actor_queue_sample(count);
    if (count == TOMBA2_ACTOR_QUEUE_CAPACITY) {
        psx_mod_counter_add("tomba2.widescreen.queue0_full_reject", 1);
        return;
    }
    if (count >= TOMBA2_ACTOR_QUEUE_STOCK_CAPACITY) {
        /* This callback precedes SLTI v0,v0,24. Change only its temporary
         * operand so the original instruction produces the enlarged bounded
         * predicate. The separate LHU into a1 (possibly still pending here)
         * and scratch count remain exact for the original increment/store. */
        cpu->gpr[2] = TOMBA2_ACTOR_QUEUE_STOCK_CAPACITY - 1u;
        psx_mod_counter_add("tomba2.widescreen.queue0_capacity_extension", 1);
    }
}

PSX_MOD_CONSTRUCTOR(tomba2_register_actor_queue_plugins) {
    static const char *const plugins[] = {
        "tomba2.widescreen.16-9", "tomba2.widescreen.21-9",
        "tomba2.widescreen.adaptive"
    };
    for (unsigned i = 0; i < sizeof plugins / sizeof plugins[0]; ++i) {
        (void)psx_mod_register_instruction_plugin(plugins[i],
            0x8003BB90u, 0xAC62013Cu, tomba2_actor_queue_reset);
        (void)psx_mod_register_instruction_plugin(plugins[i],
            0x80079BB4u, 0xAC62013Cu, tomba2_actor_queue_reset);
        for (unsigned j = 0; j < sizeof tomba2_actor_queue_producers /
                                      sizeof tomba2_actor_queue_producers[0]; ++j)
            (void)psx_mod_register_instruction_plugin(plugins[i],
                tomba2_actor_queue_producers[j].compare, 0x28420018u,
                tomba2_actor_queue_capacity);
    }
}
