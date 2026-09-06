/*
 * P1 sensor dashboard reference: combined pin mux header.
 * Every value here was copied from a verified, working SDK example
 * (see the manual's Section 4 "Where Each Pin Assignment Came From" table),
 * not invented for this project.
 */
#ifndef _PIN_MUX_H_
#define _PIN_MUX_H_

#define PORT_MUX_NO_INIT 0U

void BOARD_InitPins(void);

#endif /* _PIN_MUX_H_ */
