/**************************************************************************//**
 * @main.c
 * @brief This project demonstrates polled period measurement using input
 * capture. A periodic input signal is routed to a Compare/Capture channel,
 * and each period length is calculated from the captured edges. Connect a
 * periodic signal to the GPIO pin specified in the readme.txt for input.
 * Note maximum measurable frequency is 333 kHz.
 * @version 0.0.1
 ******************************************************************************
 * @section License
 * <b>Copyright 2020 Silicon Labs, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#include "em_device.h"
#include "em_cmu.h"
#include "em_emu.h"
#include "em_chip.h"
#include "em_gpio.h"
#include "em_timer.h"

// Default clock value
#define HFPERCLK_IN_MHZ 19

// Most recent measured period in microseconds
static volatile uint32_t measuredPeriod;

// Stored edge from previous interrupt
static volatile uint32_t lastCapturedEdge;

// Number of timer overflows since last interrupt;
static volatile uint32_t overflowCount;

/**************************************************************************//**
 * @brief
 *    GPIO initialization
 *****************************************************************************/
void initGpio(void)
{
  // Configure PA6 as input
  GPIO_PinModeSet(gpioPortA, 6, gpioModeInput, 0);
}

/**************************************************************************//**
 * @brief
 *    CMU initialization
 *****************************************************************************/
void initCmu(void)
{
  // Enable clock to GPIO and TIMER0
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_TIMER0, true);
}

/**************************************************************************//**
 * @brief
 *    TIMER initialization
 *****************************************************************************/
void initTimer(void)
{
  // Initialize the timer
  TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
  // Configure TIMER0 Compare/Capture for output compare
  TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;

  timerInit.prescale = timerPrescale1;
  timerInit.enable = false;
  timerCCInit.edge = timerEdgeFalling;
  timerCCInit.mode = timerCCModeCapture;

  TIMER_Init(TIMER0, &timerInit);

  // Route Timer0 CC0 output to PA6
  GPIO->TIMERROUTE[0].ROUTEEN  = GPIO_TIMER_ROUTEEN_CC0PEN;
  GPIO->TIMERROUTE[0].CC0ROUTE = (gpioPortA << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
                    | (6 << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);

  TIMER_InitCC(TIMER0, 0, &timerCCInit);

  // Enable the TIMER
  TIMER_Enable(TIMER0, true);
}

/**************************************************************************//**
 * @brief
 *    Calculate the period of the input waveform using Capture mode on TIMER0
 *    channel 0
 *
 * @return
 *    The period of the input waveform
 *****************************************************************************/
uint32_t calculatePeriod(void)
{
  // Read the capture value from the CC register
  uint32_t current_edge = TIMER_CaptureGet(TIMER0, 0);

  // Check if the timer overflowed (flag gets set even if interrupt not enabled)
  if (TIMER_IntGet(TIMER0) & TIMER_IF_OF) {
    overflowCount++;
    TIMER_IntClear(TIMER0, TIMER_IF_OF);
  }

  // Calculate period in microseconds, while compensating for overflows
  // Interrupt latency will affect measurements for periods below 3 microseconds (333 kHz)
  uint32_t period = (overflowCount * (TIMER_TopGet(TIMER0) + 2)
                     - lastCapturedEdge + current_edge)
                     / (HFPERCLK_IN_MHZ * (1 << timerPrescale1));

  // Record the capture value for the next period measurement calculation
  lastCapturedEdge = current_edge;

  // Reset the overflow count
  overflowCount = 0;

  return period;
}

/**************************************************************************//**
 * @brief
 *    Main function
 *****************************************************************************/
int main(void)
{
  // Chip errata
  CHIP_Init();

  // Initializations
  initCmu();
  initGpio();
  initTimer();

  while (1) {

    // Wait for a capture event to happen
    while (TIMER0->STATUS & TIMER_STATUS_ICFEMPTY0);

    // Record the period into the global variable
    measuredPeriod = calculatePeriod();
  }
}

