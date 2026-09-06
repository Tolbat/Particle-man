;; Particle-Man JagStudio/RAPTOR GPU particle integration.
;; DSP audio plays boot-generated arcade PCM effects; no music or external
;; sound asset is required.

player  equ 0
useGD   equ 0

        include "externs.inc"
        include "build/romassets.inc"
        include "JAGUAR.INC"
        include "RAPTOR.INC"
        include "RAPTOR235.INC"
        include "U235SE.INC"

        .text

raptor_video_mode                   equ vidRGB16
raptor_video_enabled                equ vidENABLE
raptor_video_VARMOD                 equ vidVARMOD_OFF

raptor_first_map_object             equ 0
raptor_map_tiles_per_y              equ 0
raptor_map_tiles_per_x              equ 0
raptor_map_height                   equ 0
raptor_map_width                    equ 0
raptor_tilesize_x                   equ 0
raptor_tilesize_y                   equ 0
raptor_tilelinesz                   equ raptor_tilesize_x/2
raptor_tilesize                     equ raptor_tilelinesz*raptor_tilesize_y
raptor_map_bitdepth                 equ 4

raptor_particle_buffer_width        equ 320
raptor_particle_buffer_height       equ 224
; Fixed allocation supports engineering builds; the release scans 64 records.
raptor_particle_pixels              equ 96
raptor_particle_drift_x             equ 0
raptor_particle_drift_y             equ 0

LIST_display                        equ 0

        move.l  #raptor_video_mode,raptor_vidmode
        move.l  #raptor_video_enabled,raptor_videnable
        move.l  #raptor_video_VARMOD,raptor_varmod
        move.l  #raptor_first_map_object,raptor_maptop_obj
        move.l  #raptor_tilesize_x,raptor_tiles_x
        move.l  #raptor_tilesize_y,raptor_tiles_y
        move.l  #raptor_map_tiles_per_x+2,raptor_tilesperx
        move.l  #raptor_map_tiles_per_y+1,raptor_tilespery
        move.l  #raptor_map_width,raptor_mapwidth
        move.l  #raptor_map_height,raptor_mapheight
        move.l  #raptor_map_bitdepth,raptor_map_tiles_bits
        move.l  #raptor_particle_pixels,raptor_maxparts
        move.l  #raptor_tilesize_x-1,raptor_tilerem_mask
        move.l  #raptor_particle_buffer_width,raptor_partbuf_x
        move.l  #raptor_particle_buffer_height,raptor_partbuf_y
        move.l  #raptor_particle_drift_x,raptor_pdriftx
        move.l  #raptor_particle_drift_y,raptor_pdrifty
        move.l  #top_of_bss,raptor_top_of_bss
        move.l  #_trashram,raptor_trashram
        move.l  #RAPTOR_MTtrash,raptor_MTwork
        move.l  #RAPTOR_user_vbi,raptor_uvbi_jmp
        move.l  #RAPTOR_POST_Object_List,raptor_poobjl
        move.l  #RAPTOR_PRE_Object_List,raptor_probjl
        move.l  #RAPTOR_font8x8,raptor_8x8_addr
        move.l  #RAPTOR_font8x16,raptor_8x16_addr
        move.l  #RAPTOR_font16x16,raptor_16x16_addr
        move.l  #RAPTOR_particle_palette,raptor_partipal
        move.l  #RAPTOR_particle_gfx,raptor_pgfx
        move.l  #RAPTOR_particle_gfxe,raptor_pgfxe
        move.l  #RAPTOR_sprite_table,raptor_spritetab
        move.l  #RAPTOR_particle_table,raptor_partitab
        move.l  #RAPTOR_MT_app_name,raptor_mtapp
        move.l  #RAPTOR_MT_file_name,raptor_mtfn
        move.l  #raptor_init_table,raptor_inittab
        move.l  #0,raptor_mapbmptiles
        jsr     RAPTOR_HWinit

        bsr     Audio_Init

        jsr     RAPTOR_start_video
        move.l  #LIST_display,d0
        jsr     RAPTOR_setlist
        jsr     RAPTOR_UPDATE_ALL
        jmp     __Z9basicmainv

        include "zero_audio.s"

RAPTOR_user_vbi:
        tst.l   jsfVsyncFlag
        beq.s   .uvbi_out
        movem.l d0-d7/a0-a6,-(a7)
        jsr     RAPTOR_UPDATE_ALL
        movem.l (a7)+,d0-d7/a0-a6
.uvbi_out:
        rts

jsfVsyncFlag: dc.l 0

RAPTOR_PRE_Object_List:
        rts
RAPTOR_POST_Object_List:
        rts

; Particle-Man targets the 128-byte Atari-compatible EEPROM configuration used
; by the current GameDrive launch path. GameDrive also supports larger EEPROM
; images, but JagStudio 1.11's bundled routines send 13-bit CAT93C86 commands
; for a 2 KB Jagtopus EEPROM. These project-local entry points keep the 64-word,
; nine-bit protocol isolated from RAPTOR and return 0 on a verified write.

PAC_EE64_GPIO_0       equ     $f14800
PAC_EE64_GPIO_1       equ     $f15000
PAC_EE64_GPIO_0OF     equ     PAC_EE64_GPIO_0-JOYSTICK
PAC_EE64_GPIO_1OF     equ     PAC_EE64_GPIO_1-JOYSTICK
PAC_EE64_READ         equ     %110000000
PAC_EE64_EWEN         equ     %100110000
PAC_EE64_WRITE        equ     %101000000
PAC_EE64_EWDS         equ     %100000000

        .globl  pacEEPROM64Read
pacEEPROM64Read:
        movem.l d1-d3/a0,-(sp)
        move.l  20(sp),d1
        bsr     pacEEPROM64ReadRaw
        movem.l (sp)+,d1-d3/a0
        rts

        .globl  pacEEPROM64Write
pacEEPROM64Write:
        movem.l d1-d4/a0,-(sp)
        move.l  24(sp),d1
        move.l  28(sp),d0
        bsr     pacEEPROM64WriteWord
        movem.l (sp)+,d1-d4/a0
        rts

pacEEPROM64WriteWord:
        move.l  d2,-(sp)
        move.w  d0,d2
        bsr     pacEEPROM64WriteRaw
        bsr     pacEEPROM64ReadRaw
        cmp.w   d0,d2
        bne.s   .write_bad
        moveq   #0,d0
        bra.s   .write_done
.write_bad:
        moveq   #1,d0
.write_done:
        move.l  (sp)+,d2
        rts

pacEEPROM64WriteRaw:
        movem.l a0/d0-d3,-(sp)
        lea     JOYSTICK,a0
        tst.w   PAC_EE64_GPIO_1OF(a0)
        move.w  #PAC_EE64_EWEN,d2
        bsr     pacEEPROM64Out9
        tst.w   PAC_EE64_GPIO_1OF(a0)
        andi.w  #$3f,d1
        ori.w   #PAC_EE64_WRITE,d1
        move.w  d1,d2
        bsr     pacEEPROM64Out9
        move.w  d0,d2
        bsr     pacEEPROM64Out16
        tst.w   PAC_EE64_GPIO_1OF(a0)

        ; The serial part permits 5 ms. A bounded 6 ms wait also behaves
        ; consistently under GameDrive firmware before the read-back check.
        move.w  #2874,d0
.write_wait:
        nop
        nop
        nop
        nop
        nop
        nop
        dbra    d0,.write_wait

        move.w  #PAC_EE64_EWDS,d2
        bsr     pacEEPROM64Out9
        tst.w   PAC_EE64_GPIO_1OF(a0)
        movem.l (sp)+,a0/d0-d3
        rts

pacEEPROM64ReadRaw:
        movem.l a0/d1-d3,-(sp)
        lea     JOYSTICK,a0
        tst.w   PAC_EE64_GPIO_1OF(a0)
        andi.w  #$3f,d1
        ori.w   #PAC_EE64_READ,d1
        move.w  d1,d2
        bsr     pacEEPROM64Out9
        moveq   #0,d0
        moveq   #15,d3
.read_loop:
        tst.w   PAC_EE64_GPIO_0OF(a0)
        nop
        move.w  (a0),d1
        lsr.w   #1,d1
        addx.w  d0,d0
        nop
        nop
        nop
        nop
        nop
        nop
        dbra    d3,.read_loop
        movem.l (sp)+,a0/d1-d3
        rts

pacEEPROM64Out16:
        rol.w   #1,d2
        moveq   #15,d3
        bra.s   pacEEPROM64OutLoop

pacEEPROM64Out9:
        rol.w   #8,d2
        moveq   #8,d3
pacEEPROM64OutLoop:
        move.w  d2,PAC_EE64_GPIO_0OF(a0)
        nop
        nop
        nop
        nop
        nop
        nop
        rol.w   #1,d2
        dbra    d3,pacEEPROM64OutLoop
        rts

        include "RAPINIT.S"
        include "jagstudio_pad_zero.inc"

        .dphrase
RAPTOR_font8x8:             incbin "ASSETS/FONTS/F_8x8.BMP"
        .dphrase
RAPTOR_font8x16:            incbin "ASSETS/FONTS/F_8x16.BMP"
        .dphrase
RAPTOR_font16x16:           incbin "ASSETS/FONTS/F_16x16.BMP"
        .dphrase
RAPTOR_particle_palette:    incbin "ASSETS/PARTIPAL.BMP"
        .dphrase

        include "build/ramassets.inc"

        .dphrase
        .bss
top_of_bss:

        .dphrase
RAPTOR_MTtrash:             .ds.b 16384

        .globl  PAC_canvas_a
        .globl  PAC_canvas_b
        .globl  _PAC_canvas_a
        .globl  _PAC_canvas_b
        .dphrase
PAC_canvas_a:
_PAC_canvas_a:              .ds.b (320/2)*224
        .dphrase
PAC_canvas_b:
_PAC_canvas_b:              .ds.b (320/2)*224

        ; Prebuilt attract pages remain outside the two gameplay pages.  Object
        ; 0 points directly at these phrase-aligned pages during the attract
        ; loop, so no visible card is stalled by construction of the next one.
        .globl  PAC_credits_gfx
        .globl  PAC_title_gfx
        .globl  PAC_scores_gfx
        .globl  _PAC_credits_gfx
        .globl  _PAC_title_gfx
        .globl  _PAC_scores_gfx
        .dphrase
PAC_credits_gfx:
_PAC_credits_gfx:           .ds.b (320/2)*224
        .dphrase
PAC_title_gfx:
_PAC_title_gfx:             .ds.b (320/2)*224
        .dphrase
PAC_scores_gfx:
_PAC_scores_gfx:            .ds.b (320/2)*224

        .globl  RAPTOR_particle_gfx
        .globl  _RAPTOR_particle_gfx
        .dphrase
RAPTOR_particle_gfx:
_RAPTOR_particle_gfx:       .ds.b (raptor_particle_buffer_width/2)*raptor_particle_buffer_height
RAPTOR_particle_gfxe:

        .globl  PAC_actor_gfx
        .globl  _PAC_actor_gfx
        .dphrase
PAC_actor_gfx:
_PAC_actor_gfx:             .ds.b 59*(16/2)*16

        .globl  PAC_wake_gfx
        .globl  _PAC_wake_gfx
        .dphrase
PAC_wake_gfx:
_PAC_wake_gfx:              .ds.b 56*(32/2)*32

        .globl  PAC_pickup_gfx
        .globl  _PAC_pickup_gfx
        .dphrase
PAC_pickup_gfx:
_PAC_pickup_gfx:            .ds.b 4*(16/2)*16

        .globl  PAC_popup_gfx
        .globl  _PAC_popup_gfx
        .dphrase
PAC_popup_gfx:
_PAC_popup_gfx:             .ds.b (64/2)*16

        .globl  PAC_sentinel_gfx
        .globl  _PAC_sentinel_gfx
        .dphrase
PAC_sentinel_gfx:
_PAC_sentinel_gfx:          .ds.b 8

        .dphrase
RAPTOR_sprite_table:        .ds.b sprite_max*sprite_tabwidth
        .globl  PAC_particle_table
        .globl  _PAC_particle_table
        .dphrase
PAC_particle_table:
_PAC_particle_table:
RAPTOR_particle_table:      .ds.b raptor_particle_pixels*particle_tabwidth
        .dphrase

_trashram:
