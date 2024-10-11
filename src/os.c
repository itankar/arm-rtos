 /****************************************************************************
* MInimal Real-time Operating System (MIROS)
* version 0.25 (matching lesson 25)
*
* This software is a teaching aid to illustrate the concepts underlying
* a Real-Time Operating System (RTOS). The main goal of the software is
* simplicity and clear presentation of the concepts, but without dealing
* with various corner cases, portability, or error handling. For these
* reasons, the software is generally NOT intended or recommended for use
* in commercial applications.
*
* Copyright (C) 2018 Miro Samek. All Rights Reserved.
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <https://www.gnu.org/licenses/>.
*
* Contact Information:
* https://www.state-machine.com
****************************************************************************/
 
 /*
 * Copyright (c) 2019, Piyush Itankar <pitankar@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "os.h"

tcb_ thread_pool[THREAD_POOL_SIZE];
tcb_ *execution_queue;
tcb_ *current_thread;
tcb_ *next_thread;

volatile unsigned long int total_tasks;
unsigned long int current_sp;
unsigned long int next_sp;
unsigned long int *sp;

/*
 * main() is assumed to be defined by the application code
 */
extern int  main();

/*
 * Enables all interrupts
 */
void enable_interrupts() {
	// Enables all interrupts
	__asm("CPSIE I");
}

/*
 * Disables all interrupts
 */
void dissable_interrupts() {
	// Disables all interrupts
	__asm("CPSID I");
}

/*
 * Sets the priority of PenSV interrupt to the lowest
 * This is because the PendSV interrupt handler is used for job scheduling
 */
void set_pendsv_priority_to_low() {

	//  Set the Priority of the PendSV interrupt to minimum
	*(unsigned long int volatile *) 0xE000ED20 |= (0xFFU << 16);
}

/*
 * Set the PendSV interrupt
 */
void set_pendsv() {
	// Set the 28th Bit to trigger PendSV interrupt
	*(unsigned long int volatile *)0xE000ED04 = (1U << 28);
}

/*
 * Configures the SysTick timer for periodic interrupts.
 */
void setup_systick_timer() {
	*(unsigned long int volatile *)0xE000E010 = 0U;
	*(unsigned long int volatile *)0xE000E014 = 15999U;
	*(unsigned long int volatile *)0xE000E018 = 0U;
	*(unsigned long int volatile *)0xE000ED20 = ((*(unsigned long int volatile *)0xE000ED20) & 0x00FFFFFF) | 0x40000000;

	*(unsigned long int volatile *)0xE000E010 |= 0x07U;
}

/*
 * Reset Interrupt Handler
 */
void reset_handler(){
	extern unsigned long int _etext, _data, _edata, _bss, _ebss;
	unsigned long int *src = &_etext, *dst = &_data;
	while(dst < &_edata) *(dst++) = *(src++);
	for(dst = &_bss; dst < &_ebss; dst++) *dst = 0;

	main();

	while(1);
}

void default_handler(){
	while(1);
}

void pendsv_handler() {
	__asm("CPSID I");
	__asm("PUSH  {r4-r11}");

	if (current_thread == NULL) {
		current_thread = execution_queue;
		current_sp = (unsigned long int)(current_thread->sp);
		__asm("LDR R10, =current_sp");
		__asm("LDR SP,[R10,#0x00]");
		__asm("POP {R4-R11}");
		__asm("CPSIE I");
		return;
	}

	if (current_thread->next == NULL) {
		next_thread = execution_queue;
	} else {
		next_thread = current_thread->next;
	}

	current_sp = (unsigned long int)&current_thread->sp;
	__asm("LDR r10, =current_sp");
	__asm("LDR r11, [r10]");
	__asm("STR sp,[r11,#0x00]");

	next_sp = (unsigned long int)(next_thread->sp);
	__asm("LDR r10, =next_sp");
	__asm("LDR sp,[r10,#0x00]");
	current_thread = next_thread;

	__asm("POP {r4-r11}");
	__asm("CPSIE I");
}

void systick_handler() {
	set_pendsv();
}

static void add_to_execution_queue(tcb_ *tcb) {
	tcb->next = execution_queue;
	execution_queue = tcb;
}

/*
 * Initializes the System Correctly
 */
void system_init() {
	dissable_interrupts();
	set_pendsv_priority_to_low();
	setup_systick_timer();

	total_tasks = 0;

	enable_interrupts();
}

/*
 * Creates a Thread for a function provided and adds it into execution queue
 */
void create_thread(function task) {
	dissable_interrupts();

	unsigned long int *sp;
	tcb_ *tcb = &thread_pool[total_tasks];  // Get the Next free TCB

	sp   = (unsigned long int *)((unsigned long int*)&(tcb->stack[TASK_STACK_SIZE]));

	*(--sp) = (1U << 24);  // Set thumb bit of XPSR
	*(--sp) = (unsigned long int)task;  // PC points to the task

	for (int i=0; i <= 13; i++)
		*(--sp) = i;

	*sp = 0xDEADBEEF;

	// Get the stack pointer for current task
	tcb->sp = sp;

	add_to_execution_queue(tcb);
	total_tasks ++;

	enable_interrupts();
}

/*
 * Cortex M4 Vector Table.
 * The table lays out all the Interrupt handlers, Main processor
 * stack pointer and the associated stack.
 *
 * NOTE: change value of MAIN_STACK_SIZE to change the main stack size
 */
static unsigned long int stack[MAIN_STACK_SIZE];
__attribute__ ((section(".isr_vector"))) void (*const vectors[])() =
{
	(void (*)())((unsigned long int)stack + sizeof(stack)),
	reset_handler,                        // The reset handler
	default_handler,                      // The NMI handler
	default_handler,                      // The hard fault handler
	default_handler,                      // The MPU fault handler
	default_handler,                      // The bus fault handler
	default_handler,                      // The usage fault handler
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	default_handler,                      // SVCall handler
	default_handler,                      // Debug monitor handler
	0,                                    // Reserved
	pendsv_handler,                       // The PendSV handler
	systick_handler,                      // The SysTick handler
	default_handler,                      // GPIO Port A
	default_handler,                      // GPIO Port B
	default_handler,                      // GPIO Port C
	default_handler,                      // GPIO Port D
	default_handler,                      // GPIO Port E
	default_handler,                      // UART0 Rx and Tx
	default_handler,                      // UART1 Rx and Tx
	default_handler,                      // SSI0 Rx and Tx
	default_handler,                      // I2C0 Master and Slave
	default_handler,                      // PWM Fault
	default_handler,                      // PWM Generator 0
	default_handler,                      // PWM Generator 1
	default_handler,                      // PWM Generator 2
	default_handler,                      // Quadrature Encoder 0
	default_handler,                      // ADC Sequence 0
	default_handler,                      // ADC Sequence 1
	default_handler,                      // ADC Sequence 2
	default_handler,                      // ADC Sequence 3
	default_handler,                      // Watchdog timer
	default_handler,                      // Timer 0 subtimer A
	default_handler,                      // Timer 0 subtimer B
	default_handler,                      // Timer 1 subtimer A
	default_handler,                      // Timer 1 subtimer B
	default_handler,                      // Timer 2 subtimer A
	default_handler,                      // Timer 2 subtimer B
	default_handler,                      // Analog Comparator 0
	default_handler,                      // Analog Comparator 1
	default_handler,                      // Analog Comparator 2
	default_handler,                      // System Control (PLL, OSC, BO)
	default_handler,                      // FLASH Control
	default_handler,                      // GPIO Port F
	default_handler,                      // GPIO Port G
	default_handler,                      // GPIO Port H
	default_handler,                      // UART2 Rx and Tx
	default_handler,                      // SSI1 Rx and Tx
	default_handler,                      // Timer 3 subtimer A
	default_handler,                      // Timer 3 subtimer B
	default_handler,                      // I2C1 Master and Slave
	default_handler,                      // Quadrature Encoder 1
	default_handler,                      // CAN0
	default_handler,                      // CAN1
	0,                                    // Reserved
	0,                                    // Reserved
	default_handler,                      // Hibernate
	default_handler,                      // USB0
	default_handler,                      // PWM Generator 3
	default_handler,                      // uDMA Software Transfer
	default_handler,                      // uDMA Error
	default_handler,                      // ADC1 Sequence 0
	default_handler,                      // ADC1 Sequence 1
	default_handler,                      // ADC1 Sequence 2
	default_handler,                      // ADC1 Sequence 3
	0,                                    // Reserved
	0,                                    // Reserved
	default_handler,                      // GPIO Port J
	default_handler,                      // GPIO Port K
	default_handler,                      // GPIO Port L
	default_handler,                      // SSI2 Rx and Tx
	default_handler,                      // SSI3 Rx and Tx
	default_handler,                      // UART3 Rx and Tx
	default_handler,                      // UART4 Rx and Tx
	default_handler,                      // UART5 Rx and Tx
	default_handler,                      // UART6 Rx and Tx
	default_handler,                      // UART7 Rx and Tx
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	default_handler,                      // I2C2 Master and Slave
	default_handler,                      // I2C3 Master and Slave
	default_handler,                      // Timer 4 subtimer A
	default_handler,                      // Timer 4 subtimer B
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	0,                                    // Reserved
	default_handler,                      // Timer 5 subtimer A
	default_handler,                      // Timer 5 subtimer B
	default_handler,                      // Wide Timer 0 subtimer A
	default_handler,                      // Wide Timer 0 subtimer B
	default_handler,                      // Wide Timer 1 subtimer A
	default_handler,                      // Wide Timer 1 subtimer B
	default_handler,                      // Wide Timer 2 subtimer A
	default_handler,                      // Wide Timer 2 subtimer B
	default_handler,                      // Wide Timer 3 subtimer A
	default_handler,                      // Wide Timer 3 subtimer B
	default_handler,                      // Wide Timer 4 subtimer A
	default_handler,                      // Wide Timer 4 subtimer B
	default_handler,                      // Wide Timer 5 subtimer A
	default_handler,                      // Wide Timer 5 subtimer B
	default_handler,                      // FPU
	default_handler,                      // PECI 0
	default_handler,                      // LPC 0
	default_handler,                      // I2C4 Master and Slave
	default_handler,                      // I2C5 Master and Slave
	default_handler,                      // GPIO Port M
	default_handler,                      // GPIO Port N
	default_handler,                      // Quadrature Encoder 2
	default_handler,                      // Fan 0
	0,                                    // Reserved
	default_handler,                      // GPIO Port P (Summary or P0)
	default_handler,                      // GPIO Port P1
	default_handler,                      // GPIO Port P2
	default_handler,                      // GPIO Port P3
	default_handler,                      // GPIO Port P4
	default_handler,                      // GPIO Port P5
	default_handler,                      // GPIO Port P6
	default_handler,                      // GPIO Port P7
	default_handler,                      // GPIO Port Q (Summary or Q0)
	default_handler,                      // GPIO Port Q1
	default_handler,                      // GPIO Port Q2
	default_handler,                      // GPIO Port Q3
	default_handler,                      // GPIO Port Q4
	default_handler,                      // GPIO Port Q5
	default_handler,                      // GPIO Port Q6
	default_handler,                      // GPIO Port Q7
	default_handler,                      // GPIO Port R
	default_handler,                      // GPIO Port S
	default_handler,                      // PWM 1 Generator 0
	default_handler,                      // PWM 1 Generator 1
	default_handler,                      // PWM 1 Generator 2
	default_handler,                      // PWM 1 Generator 3
	default_handler                       // PWM 1 Fault
};
