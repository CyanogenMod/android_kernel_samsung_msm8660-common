
/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef MIPI_S6E8AA0_WXGA_Q1_H
#define MIPI_S6E8AA0_WXGA_Q1_H

int mipi_s6e8aa0_wxga_q1_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

char* get_s6e8aa0_id_buffer( void );

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L)  || defined(CONFIG_JPN_MODEL_SC_05D) 
#define S6E8AA0_WXGA_Q1_58HZ_500MBPS
#undef S6E8AA0_WXGA_Q1_60HZ_500MBPS
#undef S6E8AA0_WXGA_Q1_57p2HZ_480MBPS
#define MAPPING_TBL_AUTO_BRIGHTNESS 
#else
#undef S6E8AA0_WXGA_Q1_58HZ_500MBPS
#undef S6E8AA0_WXGA_Q1_60HZ_500MBPS
#define S6E8AA0_WXGA_Q1_57p2HZ_480MBPS
#define MAPPING_TBL_AUTO_BRIGHTNESS 
#endif

#endif  /* MIPI_S6E8AA0_WXGA_Q1_H */
