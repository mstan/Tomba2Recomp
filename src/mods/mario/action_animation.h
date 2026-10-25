#pragma once

namespace tomba2::mario {
// Observed US native states: chest 3/8..12, hanging 4/1, and the burning
// house ladder 0xA/11 (catch), /12 (climb), /13 (exit). Motion 0x41 is
// the native sustained climb; its root movement remains entirely native.
inline bool ladder_animation(unsigned action,unsigned motion) {
    return action>=0xa && action<=0xd && motion==0x41;
}
inline unsigned traversal_animation(unsigned phase,unsigned motion) {
    switch(motion) {
    case 0x40:return 572; // ledge catch
    case 0x41:return phase==13?576:573; // climb cycle / climb out
    case 0x63:case 0x66:return 509; // approach jump
    case 0x64:case 0x67:return 513; // approach fall
    case 0x65:case 0x16:case 0x17:return 518; // landing
    default:return 499; // turning/reading a sign is not a climb
    }
}
inline unsigned capture_animation(unsigned phase,unsigned air=0) {
    // Phases8/9 perch on a chest or other supported object; 10..12 spring
    // away from it. These share the capture action but never carry/throw it.
    return phase>=10?(air==2?513:509):phase>=8?520:phase>=5?563:phase>=2?562:561;
}
inline unsigned post_animation(unsigned phase,unsigned air,unsigned previous_clip,unsigned clock) {
    // Finish the catch after native attachment phase0, then hold the ledge.
    // Jumping up from a post is an ascent before it becomes a fall.
    if(!phase || (phase==1 && previous_clip==572 && clock<10))return 572;
    return phase==1?573:air==2?513:509;
}
constexpr unsigned punch_offset=202,punch_half_width=178;
static_assert(punch_offset-punch_half_width==24);
static_assert(punch_offset+punch_half_width==380); // 2.5 * previous extent152
} // namespace tomba2::mario
