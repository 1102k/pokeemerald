STD_REVERB = 50

$(MID_BUILDDIR)/%.o: $(MID_SUBDIR)/%.s
	$(AS) $(ASFLAGS) -I sound -o $@ $<

$(MID_SUBDIR)/mus_ajito.s: %.s: %.mid
	$(MID) $< $@ -G076 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_aqa_0.s: %.s: %.mid
	$(MID) $< $@ -G065 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_ashroad.s: %.s: %.mid
	$(MID) $< $@ -G055 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_ayasii.s: %.s: %.mid
	$(MID) $< $@ -G069 -R$(STD_REVERB) -V078

$(MID_SUBDIR)/mus_b_arena.s: %.s: %.mid
	$(MID) $< $@ -G104 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_b_dome.s: %.s: %.mid
	$(MID) $< $@ -G111 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_b_dome1.s: %.s: %.mid
	$(MID) $< $@ -G111 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_naminori.s: %.s: %.mid
	$(MID) $< $@ -G017 -R$(STD_REVERB)

$(MID_SUBDIR)/mus_ropeway.s: %.s: %.mid
	$(MID) $< $@ -G071 -R$(STD_REVERB) -V078
