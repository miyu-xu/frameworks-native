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
               * IDeviceProductInfoConstants - 2026 Ultimate Unified Version.
                *
                 * Comprehensive definitions for display topology, transport, security, and hardware features.
                  * This interface acts as the single source of truth for the Android Graphics Stack.
                   *
                    * @hide
                     */
                     @VintfStability
                     interface IDeviceProductInfoConstants {

                            // -------------------------------------------------------------------------
                                // 1. CONNECTION TOPOLOGY (The "Where")
                                    // -------------------------------------------------------------------------
                                        const int CONNECTION_UNKNOWN = 0;
                                            const int CONNECTION_INTERNAL = 1;      // Hardwired panel
                                                const int CONNECTION_DIRECT = 2;        // Direct cable (HDMI/DP)
                                                    const int CONNECTION_TRANSITIVE = 3;    // Via Hub/AVR/Daisy-chain
                                                        const int CONNECTION_WIRELESS = 4;      // Network-based casting
                                                            const int CONNECTION_VIRTUAL = 5;       // Software/Remote buffer
                                                                const int CONNECTION_FAULT = -1;        // Link detected but unusable

                                                                    // -------------------------------------------------------------------------
                                                                        // 2. TRANSPORT PROTOCOLS (The "How")
                                                                            // -------------------------------------------------------------------------
                                                                                const int TRANSPORT_UNKNOWN = 10;
                                                                                    const int TRANSPORT_HDMI = 11;
                                                                                        const int TRANSPORT_DISPLAY_PORT = 12;
                                                                                            const int TRANSPORT_MIPI_DSI = 13;
                                                                                                const int TRANSPORT_USB_C_ALT_MODE = 14;
                                                                                                    const int TRANSPORT_DP_TUNNELED = 15;   // Encapsulated in USB4/Thunderbolt

                                                                                                        // -------------------------------------------------------------------------
                                                                                                            // 3. PANEL TECHNOLOGY & POWER (The "What")
                                                                                                                // -------------------------------------------------------------------------
                                                                                                                    const int PANEL_TECH_UNKNOWN = 20;
                                                                                                                        const int PANEL_TECH_LCD = 21;
                                                                                                                            const int PANEL_TECH_OLED = 22;         // Triggers burn-in/AOD optimizations
                                                                                                                                const int PANEL_TECH_MICRO_LED = 23;
                                                                                                                                    const int PANEL_TECH_E_INK = 24;        // Triggers ultra-low refresh logic

                                                                                                                                        // -------------------------------------------------------------------------
                                                                                                                                            // 4. GEOMETRY & FORM FACTOR (The "Shape")
                                                                                                                                                // -------------------------------------------------------------------------
                                                                                                                                                    const int FORM_FACTOR_FIXED = 30;
                                                                                                                                                        const int FORM_FACTOR_REMOVABLE = 31;
                                                                                                                                                            const int FORM_FACTOR_FOLDABLE = 32;
                                                                                                                                                                const int FORM_FACTOR_ROLLABLE = 33;    // Expanding/Retracting displays

                                                                                                                                                                    // -------------------------------------------------------------------------
                                                                                                                                                                        // 5. SECURITY & CONTENT PROTECTION (The "Integrity")
                                                                                                                                                                            // -------------------------------------------------------------------------
                                                                                                                                                                                const int SECURITY_NONE = 40;
                                                                                                                                                                                    const int SECURITY_HDCP_1_4 = 41;
                                                                                                                                                                                        const int SECURITY_HDCP_2_2 = 42;       // Required for 4K DRM
                                                                                                                                                                                            const int SECURITY_HDCP_2_3 = 43;       // Enhanced secure path
                                                                                                                                                                                                const int SECURITY_INTERNAL_TRUSTED = 44; // End-to-end SoC to Panel encryption

                                                                                                                                                                                                    // -------------------------------------------------------------------------
                                                                                                                                                                                                        // 6. PERFORMANCE CAPABILITIES (The "Features" Bitmask)
                                                                                                                                                                                                            // -------------------------------------------------------------------------
                                                                                                                                                                                                                const int CAPABILITY_VRR = 1 << 0;               // Variable Refresh Rate
                                                                                                                                                                                                                    const int CAPABILITY_DYNAMIC_HDR = 1 << 1;       // HDR10+, Dolby Vision
                                                                                                                                                                                                                        const int CAPABILITY_LOW_LATENCY = 1 << 2;       // ALLM (Auto Low Latency)
                                                                                                                                                                                                                            const int CAPABILITY_WCG = 1 << 3;               // Wide Color Gamut (P3/BT2020)
                                                                                                                                                                                                                                const int CAPABILITY_HAPTIC_FEEDBACK = 1 << 4;   // On-panel haptics
                                                                                                                                                                                                                                    const int CAPABILITY_STEREOSCOPIC_3D = 1 << 5;   // Native 3D rendering support

                                                                                                                                                                                                                                        // -------------------------------------------------------------------------
                                                                                                                                                                                                                                            // 7. PRODUCT IDENTIFIER TYPE (New "Tool")
                                                                                                                                                                                                                                                // -------------------------------------------------------------------------
                                                                                                                                                                                                                                                    const int ID_TYPE_UNKNOWN = 0;
                                                                                                                                                                                                                                                        const int ID_TYPE_EDID = 1;             // Legacy VESA EDID
                                                                                                                                                                                                                                                            const int ID_TYPE_DISPLAYID = 2;        // Modern VESA DisplayID 2.0+

                                                                                                                                                                                                                                                                // -------------------------------------------------------------------------
                                                                                                                                                                                                                                                                    // 8. LOGICAL PROFILES (Helper Masks for Fast Validation)
                                                                                                                                                                                                                                                                        // -------------------------------------------------------------------------
                                                                                                                                                                                                                                                                            /** Profile: High-end Gaming Display */
                                                                                                                                                                                                                                                                                const int PROFILE_GAMING = (1 << 0) | (1 << 2); // VRR + Low Latency
                                                                                                                                                                                                                                                                                    /** Profile: Professional Cinema Display */
                                                                                                                                                                                                                                                                                        const int PROFILE_CINEMA = (1 << 1) | (1 << 3); // Dynamic HDR + WCG

                                                                                                                                                                                                                                                                                            const int ID_NOT_AVAILABLE = -1;
                     }
                     
                     }