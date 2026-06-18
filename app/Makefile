empty :=
space := $(empty) $(empty)
semi := $(empty);$(empty)

ZMK_CONFIG := /workspaces/zmk-config/config

#--- Module paths ---
CORNE_MODULES := \
	/workspaces/zmk-modules/nice-view-gem

TOTEM_MODULES := \
	/workspaces/zmk-modules/zmk-totem-dongle \
	/workspaces/zmk-modules/zmk-rgbled-widget \
	/workspaces/zmk-modules/zmk-dongle-screen

#--- Miryoku/Keymap Defines ---

CORNE_KEYMAP_FLAGS := \
	-DMIRYOKU_ALPHAS_COLEMAK \
	-DMIRYOKU_CLIPBOARD_MAC \
	-DMIRYOKU_EXTRA_QWERTY \
	-DMIRYOKU_TAP_COLEMAK

TOTEM_KEYMAP_FLAGS := \
	$(CORNE_KEYMAP_FLAGS) \
	-DMIRYOKU_KLUDGE_GLOBALSHIFTFUNCTIONS \
	-DRGBLED_WIDGET_BEHAVIORS

# --- Reusable Build Macro ---
# Arguments: 1=Board, 2=Shield, 3=Output Dir, 4=Modules, 5=Flags
define build_zmk
	@west build -b $(1) -d build/$(3) -- \
		-DSHIELD="$(2)" \
		-DZMK_CONFIG="$(ZMK_CONFIG)" \
		-DZMK_EXTRA_MODULES="$(subst $(space),$(semi),$(4))" \
		$(if $(5),-DDTS_EXTRA_CPPFLAGS="$(subst $(space),$(semi),$(5))",)
endef

.PHONY: all totem corne clean

all: totem corne

# --- Totem Build Target ---
totem:
	$(call build_zmk,seeeduino_xiao_ble,totem_left rgbled_adapter,totem/left,$(TOTEM_MODULES),$(TOTEM_KEYMAP_FLAGS))
	$(call build_zmk,seeeduino_xiao_ble,totem_right rgbled_adapter,totem/right,$(TOTEM_MODULES),$(TOTEM_KEYMAP_FLAGS))
	$(call build_zmk,seeeduino_xiao_ble,totem_dongle dongle_screen,totem/dongle,$(TOTEM_MODULES),$(TOTEM_KEYMAP_FLAGS))

# --- Corne Build Target ---
corne:
	$(call build_zmk,nice_nano_v2,corne_left nice_view_adapter nice_view_gem,corne/left,$(CORNE_MODULES),$(CORNE_KEYMAP_FLAGS))
	$(call build_zmk,nice_nano_v2,corne_right nice_view_adapter nice_view_gem,corne/right,$(CORNE_MODULES),$(CORNE_KEYMAP_FLAGS))

clean:
	@rm -rf build
