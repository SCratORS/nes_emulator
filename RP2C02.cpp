#include "RP2C02.h"
#include "BUS.h"
#include <cmath>

RP2C02::RP2C02() {FrameBuffer = new uint32_t[256*240];}

uint8_t	 RP2C02::MemAccess(uint16_t addr, uint8_t data, bool write) {
	uint8_t result = 0x00;
	switch (addr & 0x07) {
		case 0x00:	if (!write) break;
					control.reg = data;
					tram_addr.nametable_x = control.nametable_x;
					tram_addr.nametable_y = control.nametable_y;
					break;
		case 0x01:	if (write) mask.reg = data;	break;
		case 0x02:	if (write) break;
					result = (status.reg & 0xE0) | (ppu_data_buffer & 0x1F);
					status.vertical_blank = 0;
					address_latch = 0;
					break;
		case 0x03:	if (write) oam_addr = data;	break;
		case 0x04:	if (write) pOAM[oam_addr] = data;
					else result =  pOAM[oam_addr];
					break;
		case 0x05:	if (!write) break;
					if (address_latch = !address_latch) { fine_x = data & 0x07; tram_addr.coarse_x = data >> 3; }
					else { tram_addr.fine_y = data & 0x07; tram_addr.coarse_y = data >> 3; }
					break;
		case 0x06:	if (!write) break;
					if (address_latch = !address_latch)	tram_addr.reg = (uint16_t)((data & 0x3F) << 8) | (tram_addr.reg & 0x00FF);
					else { tram_addr.reg = (tram_addr.reg & 0xFF00) | data; vram_addr = tram_addr; }
					break;
		case 0x07:	result = ppu_data_buffer;
					uint8_t t = PPUMemAccess(vram_addr.reg, write?data:result, write);
					if (write) result = t;
					else {
						ppu_data_buffer = t;
						if (vram_addr.reg >= 0x3F00) result = ppu_data_buffer = (ppu_data_buffer & 0xC0) | (t & 0x3F);
					};
					vram_addr.reg += control.increment_mode ? 32 : 1;
					break;
	}
	return result;
}

uint8_t RP2C02::PPUMemAccess(uint16_t addr, uint8_t data, bool write) {
	addr &= 0x3FFF;
	uint8_t result = data;
	if (addr >= 0x3F00) {	if(addr%4 == 0) addr &= 0x0F;
							uint8_t& M = tblPalette[addr&0x1F];
							if (write) M = data; else return(M & (mask.grayscale ? 0x30 : 0x3F)); }
	else if (addr >= 0x2000) {	addr &= 0x0FFF;
							uint8_t& M = tblName[(CART->Mirror() >> ((addr>>10)&0x03))&0x01][addr & 0x03FF];
							if  (write) M = data; else return M;}
	else CART->PPUMemAccess(addr, result, write);
	return result;
}

void RP2C02::reset() {
	fine_x = 0x00;
	address_latch = false;
	ppu_data_buffer = 0x00;
	scanline = -1;
	cycle = 0;
	bg_next_tile_attrib = 0x00;
	bg_shifter_pattern_lo = bg_shifter_pattern_hi = 0x0000;
	bg_shifter_attrib_lo = bg_shifter_attrib_hi = 0x0000;
	status.reg = 0x00;
	mask.reg = 0x00;
	control.reg = 0x00;
	vram_addr.reg = 0x0000;
	tram_addr.reg = 0x0000;
	odd_frame = false;
}

void RP2C02::rendering_tick() {
	bool render_SB = (mask.render_background || mask.render_sprites);
	uint8_t nOAMEntry = 0;
	if (cycle==001 && scanline < 0) status.reg = 0;
	if (cycle==304 && scanline < 0 && mask.render_background) vram_addr.reg = tram_addr.reg;
	if (cycle==339 && scanline < 0 && mask.render_background && odd_frame) {odd_frame = cycle = scanline = 0;}
	if ((cycle > 1 && cycle < 258) || (cycle > 320 && cycle < 338)) {
		if (mask.render_background) {
			bg_shifter_pattern_lo <<= 1;
			bg_shifter_pattern_hi <<= 1;
			bg_shifter_attrib_lo <<= 1;
			bg_shifter_attrib_hi <<= 1;
		}
		if (mask.render_sprites && cycle < 258) for (int i = 0; i < sprite_count; i++) {
			if (spriteScanline[i].x > 0) spriteScanline[i].x--;
			else {
				sprite_shifter_pattern_lo[i] <<= 1;
				sprite_shifter_pattern_hi[i] <<= 1;
			}
		}
		if ((cycle-1)%8 == 7 && render_SB && !(++vram_addr.coarse_x)%31) vram_addr.nametable_x = ~vram_addr.nametable_x;
		if ((cycle-1)%8 == 0) {
			bg_shifter_pattern_lo |= PPUMemAccess(bg_next_tile_addr);
			bg_shifter_pattern_hi |= PPUMemAccess(bg_next_tile_addr + 8);
			bg_shifter_attrib_lo  = (bg_shifter_attrib_lo & 0xFF00) | ((bg_next_tile_attrib & 0b01) ? 0xFF : 0x00);
			bg_shifter_attrib_hi  = (bg_shifter_attrib_hi & 0xFF00) | ((bg_next_tile_attrib & 0b10) ? 0xFF : 0x00);
			bg_next_tile_attrib = PPUMemAccess(0x23C0 + (vram_addr.reg&0xC00) + 8*(vram_addr.coarse_y/4) + (vram_addr.coarse_x/4));		
			if (vram_addr.coarse_y & 0x02) bg_next_tile_attrib >>= 4;
			if (vram_addr.coarse_x & 0x02) bg_next_tile_attrib >>= 2;
			bg_next_tile_attrib &= 0x03;
			bg_next_tile_addr = (control.pattern_background << 12) + (PPUMemAccess(0x2000 | (vram_addr.reg & 0xFFF)) << 4) + vram_addr.fine_y;
		}
	}

	if (cycle == 257) {
		if (render_SB) {
			vram_addr.nametable_x = tram_addr.nametable_x;
			vram_addr.coarse_x    = tram_addr.coarse_x;
			if (!(++vram_addr.fine_y)%7)
				if (vram_addr.coarse_y == 29) {
					vram_addr.coarse_y = 0;
					vram_addr.nametable_y ^= 1;
				} else (++vram_addr.coarse_y)%31;
		}
		if (scanline  < 0) return;
		std::memset(spriteScanline, 0xFF, MAX_SPRITE * sizeof(sObjectAttributeEntry));
		sprite_count = 0;
		bSpriteZeroHitPossible = false;
		while (nOAMEntry < 64 && sprite_count < MAX_SPRITE)	{
			int16_t diff = ((int16_t)scanline - (int16_t)OAM[nOAMEntry].y);			
			if (diff >= 0 && diff < (control.sprite_size ? 16 : 8)) {
				if (nOAMEntry == 0) bSpriteZeroHitPossible = true;
				memcpy(&spriteScanline[sprite_count], &OAM[nOAMEntry], sizeof(sObjectAttributeEntry));	
				uint8_t a = !(((scanline - spriteScanline[sprite_count].y<8)>0)^((spriteScanline[sprite_count].attribute&0x80)>0));
				uint16_t sprite_pattern_addr = 
				  ((control.sprite_size?(spriteScanline[sprite_count].id & 0x01):control.pattern_sprite) << 12) |
				  ((control.sprite_size?((spriteScanline[sprite_count].id & 0xFE) + a):spriteScanline[sprite_count].id) <<  4) |
				  (((scanline - spriteScanline[sprite_count].y)&(control.sprite_size?0x07:0xFF))^
				  	((spriteScanline[sprite_count].attribute & 0x80)?7:0));
				sprite_shifter_pattern_lo[sprite_count] = PPUMemAccess(sprite_pattern_addr);
				sprite_shifter_pattern_hi[sprite_count] = PPUMemAccess(sprite_pattern_addr + 8);
				if (spriteScanline[sprite_count].attribute & 0x40) {
					auto flipbyte = [](uint8_t b)	{
						b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
						b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
						b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
						return b;
					};
					sprite_shifter_pattern_lo[sprite_count] = flipbyte(sprite_shifter_pattern_lo[sprite_count]);
					sprite_shifter_pattern_hi[sprite_count] = flipbyte(sprite_shifter_pattern_hi[sprite_count]);
				}
				sprite_count++;					
			}
			nOAMEntry++;
		}
		status.sprite_overflow = (sprite_count > MAX_SPRITE);
	}
}

void RP2C02::rendering_pixel() {
	uint8_t pixel = 0x00, palette = 0x00;
	bool render_bg = mask.render_background && (mask.render_background_left || (cycle > 8));
	bool render_sp = mask.render_sprites && (mask.render_sprites_left || (cycle > 8));
	if (render_bg) {
			uint16_t bit_mux = 0x8000 >> fine_x;
			uint8_t p0_pixel = (bg_shifter_pattern_lo & bit_mux) > 0;
			uint8_t p1_pixel = (bg_shifter_pattern_hi & bit_mux) > 0;
			uint8_t bg_pal0 = (bg_shifter_attrib_lo & bit_mux) > 0;
			uint8_t bg_pal1 = (bg_shifter_attrib_hi & bit_mux) > 0;
			pixel = (p1_pixel << 1) | p0_pixel;
			palette = pixel ? (bg_pal1 << 1) | bg_pal0 : 0x00; 
	}
	if (render_sp) {
		bSpriteZeroBeingRendered = false;
		for (uint8_t i = 0; i < sprite_count; i++) {
			if (spriteScanline[i].x == 0) {
				uint8_t fg_pixel_lo = (sprite_shifter_pattern_lo[i] & 0x80) > 0;
				uint8_t fg_pixel_hi = (sprite_shifter_pattern_hi[i] & 0x80) > 0;
				uint8_t fg_pixel = (fg_pixel_hi << 1) | fg_pixel_lo;
				if (fg_pixel) {
					if (!(spriteScanline[i].attribute&0x20) || !pixel) {
						pixel = fg_pixel;
						palette = (spriteScanline[i].attribute & 0x03) + 0x04;
					}
					if ((i == 0) && bSpriteZeroHitPossible) bSpriteZeroBeingRendered = true;
					break;
				}
			}
		}
	}
	if (bSpriteZeroHitPossible && bSpriteZeroBeingRendered && render_bg && render_sp) status.sprite_zero_hit = 1;
	if (scanline < 0) return;
	FrameBuffer[scanline * 256 + cycle-1] = palScreen[PPUMemAccess(0x3F00+(palette<<2)+pixel)&0x3F];
}

void RP2C02::clock() {
	if (scanline < 240) {
		rendering_tick();
		if (cycle > 0 && cycle < 257) rendering_pixel();		
	}
	CART->IRQScanline(cycle, scanline, mask.reg, control.reg);
	if (++cycle >= 341)	{
		cycle = 0;
		switch (++scanline) {
			case 240: 	status.vertical_blank = 1; break;
			case 241:	if (control.enable_nmi) nmi = true; break;
			case 261:	scanline = -1;
						frame_complete = odd_frame = true;
			break;
		}
	}
}