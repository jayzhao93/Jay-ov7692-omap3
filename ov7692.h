#ifndef _MEDIA_OV7692_H
#define _MEDIA_OV7692_H

struct ov7692_platform_data {
       	unsigned int clk_pol:1;
       
       	const s64 *link_freqs;
       	s64 link_def_freq;
       };

#endif
