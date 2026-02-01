/*
 * Copyright (C) 2026 The Android Open Source Project
  *
   * Licensed under the Apache License, Version 2.0 (the "License");
    * you may not use this file except in compliance with the License.
     * You may obtain a copy of the License at
      *
       * http://www.apache.org/licenses/LICENSE-2.0
        *
         * Unless required by applicable law or agreed to in writing, software
          * distributed under the License is distributed on an "AS IS" BASIS,
           * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
            * See the License for the specific language governing permissions and
             * limitations under the License.
              */

              package android.hardware.display;

              /**
               * IDeviceProductInfoConstants
                *
                 * Global constants for display device metadata, including topology, 
                  * transport protocols, panel technologies, and security.
                   *
                    * @hide
                     */
                     @VintfStability
                     interface IDeviceProductInfoConstants {

                            // 1. CONNECTION TOPOLOGY
                                const int CONNECTION_UNKNOWN = 0;
                                    const int CONNECTION_INTERNAL = 1;
                                        const int CONNECTION_DIRECT = 2;
                                            const int CONNECTION_TRANSITIVE = 3;
                                                const int CONNECTION_WIRELESS = 4;
                                                    const int CONNECTION_VIRTUAL = 5;
                                                        const int CONNECTION_FAULT = -1;

                                                            // 2. TRANSPORT PROTOCOLS
                                                                const int TRANSPORT_UNKNOWN = 10;
                                                                    const int TRANSPORT_HDMI = 11;
                                                                        const int TRANSPORT_DISPLAY_PORT = 12;
                                                                            const int TRANSPORT_MIPI_DSI = 13;
                                                                                const int TRANSPORT_USB_C_ALT_MODE = 14;
                                                                                    const int TRANSPORT_DP_TUNNELED = 15;

                                                                                        // 3. PANEL TECHNOLOGY
                                                                                            const int PANEL_TECH_UNKNOWN = 20;
                                                                                                const int PANEL_TECH_LCD = 21;
                                                                                                    const int PANEL_TECH_OLED = 22;
                                                                                                        const int PANEL_TECH_MICRO_LED = 23;
                                                                                                            const int PANEL_TECH_E_INK = 24;

                                                                                                                // 4. PHYSICAL FORM FACTOR
                                                                                                                    const int FORM_FACTOR_FIXED = 30;
                                                                                                                        const int FORM_FACTOR_REMOVABLE = 31;
                                                                                                                            const int FORM_FACTOR_FOLDABLE = 32;
                                                                                                                                const int FORM_FACTOR_ROLLABLE = 33;

                                                                                                                                    // 5. SECURITY & CONTENT PROTECTION
                                                                                                                                        const int SECURITY_NONE = 40;
                                                                                                                                            const int SECURITY_HDCP_1_4 = 41;
                                                                                                                                                const int SECURITY_HDCP_2_2 = 42;
                                                                                                                                                    const int SECURITY_HDCP_2_3 = 43;
                                                                                                                                                        const int SECURITY_INTERNAL_TRUSTED = 44;

                                                                                                                                                            // 6. CAPABILITY BITMASK
                                                                                                                                                                const int CAPABILITY_VRR = 1 << 0;
                                                                                                                                                                    const int CAPABILITY_DYNAMIC_HDR = 1 << 1;
                                                                                                                                                                        const int CAPABILITY_LOW_LATENCY = 1 << 2;
                                                                                                                                                                            const int CAPABILITY_WCG = 1 << 3;
                                                                                                                                                                                const int CAPABILITY_HAPTIC_FEEDBACK = 1 << 4;

                                                                                                                                                                                    // 7. SERVICE CONSTANTS
                                                                                                                                                                                        const int ID_NOT_AVAILABLE = -1;
                     }
                     
                     }