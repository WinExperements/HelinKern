#ifndef MSM_GENERIC_H
#define MSM_GENERIC_H

// Qualcomm Generic Platform Header file.
// This header file defines all msm_ functions
// Initialize specific SoC
void msm_board_init();
// Initialize SoC specific timer. Required by the scheduler as core component.
void msm_timer_init();

#endif
