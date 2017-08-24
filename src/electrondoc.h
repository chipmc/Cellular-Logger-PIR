/**********************************************************************************************************************
 * Copyright © 2016 by Geert Van Hecke
 *
 * Particle Electron pinout source code documentation
 *
 *                                                             ┌─────┐
 *                                             ┌───────┬─────┬─┤ USB ├───────┐
 *                                             │ ● Li+ │ • • │ └─────┘VUSB ● │
 *                                            ─┤ ○ VIN └─────┘         3V3 ○ ├─
 *                                            ─┤ ○ GND                 RST ○ ├─
 *                                            ─┤ ○ TX                 VBAT ○ ├─
 *                                            ─┤ ○ RX                  GND ○ ├─
 *                                            ─┤ ○ WKP  ◙     □     ◙   D7 ○ ├─
 *                                            ─┤ ○ DAC  MODE    RESET   D6 ○ ├─
 *                                            ─┤ ○ A5 ┌───────────────┐ D5 ○ ├─
 *                                            ─┤ ○ A4 │    Electron   │ D4 ○ ├─
 *                                            ─┤ ○ A3 │               │ D3 ○ ├─
 *                                            ─┤ ○ A2 │               │ D2 ○ ├─
 *                                            ─┤ ○ A1 │               │ D1 ○ ├─
 *                                            ─┤ ○ A0 │               │ D0 ○ ├─
 *                                            ─┤ ○ B5 │               │ C5 ○ ├─
 *                                            ─┤ ○ B4 │               │ C4 ○ ├─
 *                                            ─┤ ○ B3 │               │ C3 ○ ├─
 *                                            ─┤ ○ B2 │ µblox         │ C2 ○ ├─
 *                                            ─┤ ○ B1 └───────────────┘ C1 ○ ├─
 *                                            ─┤ ○ B0    ┌───┐          C0 ○ ├─
 *                                             │         │ ○ │               │
 *                                             │         └───┘           *   │
 *                                              \___________________________/
 *
 *
 * D0 -
 * D1 -
 * D3 - PIR Digital Signal (active high) - intPin
 * D6 - Done Pin - connected to Watchdog Timer
 * D7 - LED on Electron board - LEDPIN
 * A0 - TMP36 Input -  tmp36Pin
 * B5 - TMP36 Shutdown Pin
 * A7 - WDT Pin from Watchdog Timer
 *
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
***********************************************************************************************************************/


/**
 * @file   doc.h
 * @author Geert Van Hecke
 * @date   12 April 2016
 * @brief  File containing the pinout documentation of a Particle Electron.
 *
 * Here typically goes a more extensive explanation of what the header defines.
 */
