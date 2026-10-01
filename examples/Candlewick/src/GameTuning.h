#pragma once
#include <glm/glm.hpp>

#include <cstdint>

namespace Dingo
{
	inline constexpr const char* SCENE_TITLE   = "Title";
	inline constexpr const char* SCENE_KEEP    = "Keep";
	inline constexpr const char* SCENE_END     = "End";

	inline constexpr float HUD_ORTHO_SIZE      = 11.0f;
	inline constexpr float HUD_PADDING         = 0.45f;
	inline constexpr float HUD_ROOM_LABEL_SIZE = 0.55f;
	inline constexpr float HUD_ROOM_LABEL_DROP = 0.95f;
	inline constexpr float HUD_OIL_BAR_WIDTH   = 4.5f;
	inline constexpr float HUD_OIL_BAR_HEIGHT  = 0.46f;
	inline constexpr float HUD_OIL_BAR_INSET   = 0.07f;
	inline constexpr float HUD_OIL_BAR_RISE    = 1.1f;
	inline constexpr float HUD_OIL_STATE_SIZE  = 0.44f;
	inline constexpr float HUD_OIL_STATE_RISE  = 0.42f;
	inline constexpr float HUD_OIL_HINT_SIZE   = 0.32f;
	inline constexpr float HUD_OIL_HINT_DROP   = 0.78f;
	inline constexpr float HUD_OIL_FILL_LIFT   = 0.1f;
	inline constexpr float HUD_FADE_MARGIN     = 1.0f;
	inline constexpr float HUD_PROMPT_SIZE     = 0.5f;
	inline constexpr float HUD_PROMPT_RISE     = 2.3f;
	inline constexpr float HUD_PROGRESS_WIDTH  = 3.6f;
	inline constexpr float HUD_PROGRESS_HEIGHT = 0.3f;
	inline constexpr float HUD_PROGRESS_INSET  = 0.05f;
	inline constexpr float HUD_PROGRESS_RISE   = 1.65f;
	inline constexpr float HUD_TOAST_SIZE      = 0.75f;
	inline constexpr float HUD_TOAST_DROP      = 1.3f;
	inline constexpr float HUD_TOAST_FADE_TIME = 0.5f;
	inline constexpr float HUD_PAUSE_Z         = 0.45f;
	inline constexpr float HUD_PAUSE_TITLE_SIZE = 1.2f;
	inline constexpr float HUD_PAUSE_TITLE_RISE = 0.9f;
	inline constexpr float HUD_PAUSE_HINT_SIZE = 0.42f;
	inline constexpr float HUD_PAUSE_HINT_DROP = 0.3f;

	inline constexpr float TITLE_HEADING_SIZE  = 1.5f;
	inline constexpr float TITLE_HEADING_Y     = 2.4f;
	inline constexpr float TITLE_TAGLINE_SIZE  = 0.42f;
	inline constexpr float TITLE_TAGLINE_Y     = 0.5f;
	inline constexpr float TITLE_PROMPT_SIZE   = 0.5f;
	inline constexpr float TITLE_PROMPT_Y      = -2.2f;
	inline constexpr float TITLE_CONTROLS_SIZE = 0.34f;
	inline constexpr float TITLE_CONTROLS_Y    = -3.7f;
	inline constexpr float END_HEADING_SIZE    = 1.2f;
	inline constexpr float END_HEADING_Y       = 1.8f;
	inline constexpr float END_TIME_SIZE       = 0.7f;
	inline constexpr float END_TIME_Y          = 0.4f;
	inline constexpr float END_CATCHES_SIZE    = 0.48f;
	inline constexpr float END_CATCHES_Y       = -0.55f;
	inline constexpr float END_PROMPT_SIZE     = 0.45f;
	inline constexpr float END_PROMPT_Y        = -2.6f;

	// --- Camera (three-quarter view, fixed yaw) --------------------------------
	inline constexpr float CAMERA_FOV          = 50.0f;
	inline constexpr float CAMERA_PITCH_DEG    = 55.0f;
	inline constexpr float CAMERA_YAW_DEG      = 0.0f;
	inline constexpr float CAMERA_DISTANCE     = 10.0f;
	inline constexpr float CAMERA_FOCUS_HEIGHT = 0.8f;
	inline constexpr float CAMERA_LAG          = 6.0f;
	inline constexpr float CAMERA_NEAR         = 0.1f;
	inline constexpr float CAMERA_FAR          = 160.0f;
	inline constexpr float OVERVIEW_MARGIN     = 0.94f;

	// --- Keep geometry (1 tile = 1 m) ---------------------------------------------
	inline constexpr float TILE_SIZE           = 1.0f;
	inline constexpr float WALL_HEIGHT         = 2.0f;
	inline constexpr float FLOOR_THICKNESS     = 0.2f;
	inline constexpr int   WALL_MAX_RUN        = 4;
	inline constexpr float WALL_CAP_THICKNESS  = 0.08f;
	inline constexpr float WALL_CAP_OVERHANG   = 0.04f;
	inline constexpr float CUTAWAY_PADDING     = 0.15f;
	inline constexpr float CUTAWAY_TARGET_HEIGHT = 0.2f;

	// --- Player --------------------------------------------------------------------
	inline constexpr float PLAYER_SPEED        = 3.0f;
	inline constexpr float PLAYER_RADIUS       = 0.3f;
	inline constexpr float PLAYER_HEIGHT       = 1.7f;
	inline constexpr float PLAYER_STEP         = 0.3f;
	inline constexpr float PLAYER_SLOPE        = 45.0f;
	inline constexpr float PLAYER_SPAWN_LIFT   = 0.05f;
	inline constexpr float PLAYER_TURN_SPEED   = 14.0f;
	inline constexpr float GRAVITY_Y           = -18.0f;

	// --- Lantern --------------------------------------------------------------------
	inline constexpr float OIL_MAX               = 100.0f;
	inline constexpr float OIL_BURN_PER_SECOND   = 1.0f;
	inline constexpr float OIL_RELIGHT_COST      = 3.0f;
	inline constexpr float OIL_PER_FLASK         = 35.0f;
	inline constexpr float FLASK_PICKUP_RADIUS   = 1.0f;
	inline constexpr float LANTERN_STRIKE_TIME   = 0.8f;
	inline constexpr float LANTERN_RANGE_MIN     = 2.5f;
	inline constexpr float LANTERN_RANGE_MAX     = 7.0f;
	inline constexpr float LANTERN_INTENSITY     = 2.0f;
	inline constexpr glm::vec3 LANTERN_COLOR     = { 1.0f, 0.72f, 0.42f };
	inline constexpr float LANTERN_FLICKER_OIL   = 5.0f;
	inline constexpr float LANTERN_FLICKER_DEPTH = 0.4f;
	inline constexpr float LANTERN_EMISSIVE_MIN  = 0.4f;
	inline constexpr float LANTERN_EMISSIVE_MAX  = 1.1f;

	// --- Wardens --------------------------------------------------------------------
	inline constexpr float WARDEN_PATROL_SPEED      = 1.6f;
	inline constexpr float WARDEN_INVESTIGATE_SPEED = 2.4f;
	inline constexpr float WARDEN_TURN_SPEED        = 4.0f;
	inline constexpr float WARDEN_WAYPOINT_PAUSE    = 0.8f;
	inline constexpr float WARDEN_LOOK_TIME         = 3.0f;
	inline constexpr float WARDEN_LOOK_SWEEP_DEG    = 60.0f;
	inline constexpr float WARDEN_PATH_CLEARANCE    = 0.3f;
	inline constexpr float WARDEN_INVESTIGATE_STOP  = 1.0f;
	inline constexpr float WARDEN_YIELD_DISTANCE    = 0.9f;
	inline constexpr float WARDEN_YIELD_AHEAD       = 0.5f;

	inline constexpr float WARDEN_EYE_HEIGHT        = 1.7f;
	inline constexpr float WARDEN_EYE_FORWARD       = 0.25f;
	inline constexpr float WARDEN_EYE_PITCH_DEG     = 25.0f;
	inline constexpr float WARDEN_EYE_RANGE         = 8.0f;
	inline constexpr float WARDEN_EYE_RANGE_MARGIN  = 0.5f;
	inline constexpr float WARDEN_EYE_RANGE_GROWTH  = 6.0f;
	inline constexpr float WARDEN_EYE_INNER_DEG     = 14.0f;
	inline constexpr float WARDEN_EYE_OUTER_DEG     = 24.0f;
	inline constexpr float WARDEN_EYE_INTENSITY     = 1.4f;
	inline constexpr glm::vec3 WARDEN_LIGHT_COLOR   = { 0.7f, 0.8f, 1.0f };

	inline constexpr float WARDEN_LAMP_RANGE        = 3.5f;
	inline constexpr float WARDEN_LAMP_INTENSITY    = 0.9f;
	inline constexpr glm::vec3 WARDEN_LAMP_OFFSET   = { 0.42f, 1.0f, -0.16f };
	inline constexpr float WARDEN_LAMP_EMISSIVE     = 1.0f;
	inline constexpr float WARDEN_MARKER_HEIGHT     = 2.2f;
	inline constexpr float WARDEN_MARKER_SIZE       = 0.18f;
	inline constexpr float WARDEN_MARKER_EMISSIVE   = 1.2f;

	// --- Detection ------------------------------------------------------------------
	// The feet sample's weight where the eye's pool on the floor fades below 3/255 of added light
	// (6.4 m along the axis at range 8), so standing in the visible pool is being seen.
	inline constexpr float SEEN_WEIGHT              = 0.1f;
	inline constexpr float SAMPLE_FEET              = 0.1f;
	inline constexpr float SAMPLE_CHEST             = 1.0f;
	inline constexpr float SAMPLE_HEAD              = 1.6f;
	inline constexpr float SIGHT_SLACK              = PLAYER_RADIUS;
	inline constexpr float SUSPICION_CONE_BASE      = 0.6f;
	inline constexpr float SUSPICION_CONE_SCALE     = 1.2f;
	inline constexpr float SUSPICION_BEACON_RATE    = 0.5f;
	inline constexpr float SUSPICION_DECAY          = 0.25f;
	inline constexpr float SUSPICION_INVESTIGATE    = 0.4f;
	inline constexpr float SUSPICION_ALERT          = 0.75f;
	inline constexpr float SUSPICION_TOUCH          = 0.6f;
	inline constexpr float TOUCH_DISTANCE           = 0.8f;
	inline constexpr float BEACON_LANTERN_SCALE     = 1.6f;
	inline constexpr float BEACON_FLAME_RANGE       = 6.0f;
	inline constexpr float BEACON_FIELD_DEG         = 120.0f;
	inline constexpr float BEACON_FLAME_WEIGHT      = 0.25f;
	inline constexpr float CAUGHT_FADE_TIME         = 1.0f;
	inline constexpr float RESPAWN_FADE_TIME        = 0.5f;
	inline constexpr float RESPAWN_GRACE_TIME       = 2.5f;

	// --- Detection debug view (--debug-cone) ----------------------------------------
	inline constexpr uint32_t DEBUG_CONE_SEGMENTS   = 24;
	inline constexpr float DEBUG_DOT_SPACING        = 0.25f;
	inline constexpr float DEBUG_DOT_SIZE           = 0.07f;
	inline constexpr float DEBUG_DOT_LIFT           = 0.012f;
	inline constexpr float DEBUG_SAMPLE_SIZE        = 0.12f;
	inline constexpr float DEBUG_SAMPLE_OFFSET      = 0.4f;

	// --- Light LOD (decorative flames only) ------------------------------------------
	inline constexpr int   LIGHT_LOD_HEADROOM    = 2;
	inline constexpr float LIGHT_LOD_STICKINESS  = 0.6f;
	inline constexpr float LIGHT_LOD_FADE_TIME   = 0.3f;
	inline constexpr float LIGHT_LOD_VIEW_MARGIN = 2.0f;

	// The lantern, four braziers and the altar, and four wardens' lamp and eye: every gameplay light that can burn at once.
	inline constexpr int   GAMEPLAY_LIGHTS_MAX   = 1 + 5 + 4 * 2;
	inline constexpr int   LIGHT_BUDGET_MIN      = GAMEPLAY_LIGHTS_MAX + LIGHT_LOD_HEADROOM;

	// --- Ambient: the keep has no sun ------------------------------------------
	inline constexpr glm::vec3 AMBIENT_COLOR   = { 0.55f, 0.6f, 1.0f };
	inline constexpr float AMBIENT_INTENSITY   = 0.03f;

	// --- Flames --------------------------------------------------------------------
	inline constexpr glm::vec3 FLAME_COLOR     = { 1.0f, 0.62f, 0.3f };
	inline constexpr float FLAME_EMISSIVE      = 1.1f;

	inline constexpr float BRAZIER_LIGHT_RANGE     = 9.0f;
	inline constexpr float BRAZIER_LIGHT_INTENSITY = 1.1f;
	inline constexpr float ALTAR_LIGHT_RANGE       = 11.0f;
	inline constexpr float ALTAR_LIGHT_INTENSITY   = 1.2f;

	inline constexpr float SCONCE_LIGHT_RANGE      = 2.5f;
	inline constexpr float SCONCE_LIGHT_INTENSITY  = 0.8f;
	inline constexpr float SCONCE_HEIGHT           = 1.45f;

	inline constexpr float CANDLE_LIGHT_RANGE      = 2.5f;
	inline constexpr float CANDLE_LIGHT_INTENSITY  = 0.8f;

	inline constexpr uint32_t FLAME_MESH_RINGS     = 6;
	inline constexpr uint32_t FLAME_MESH_SEGMENTS  = 8;

	// --- Braziers: checkpoints and the win -----------------------------------------
	inline constexpr float BRAZIER_REACH           = 1.5f;
	inline constexpr float BRAZIER_LIGHT_TIME      = 1.0f;
	inline constexpr float CHECKPOINT_TOAST_TIME   = 2.0f;
	inline constexpr float WIN_LINGER_TIME         = 1.2f;
	inline constexpr float WIN_FADE_TIME           = 1.4f;
	inline constexpr float CHECKPOINT_OIL_MARGIN   = 2.0f;
	inline constexpr float CHECKPOINT_MIN_OIL      = OIL_RELIGHT_COST + CHECKPOINT_OIL_MARGIN;

	// Unrelated sine rates keep a flame from visibly repeating; a phase of its own keeps neighbours out of step.
	inline constexpr float FLICKER_RATE_A          = 9.0f;
	inline constexpr float FLICKER_RATE_B          = 14.3f;
	inline constexpr float FLICKER_RATE_B_PHASE    = 1.7f;
	inline constexpr float BRAZIER_FLICKER_DEPTH   = 0.08f;
	inline constexpr float BRAZIER_FLICKER_PHASE   = 1.9f;
	inline constexpr float DECOR_FLICKER_DEPTH     = 0.05f;
	inline constexpr float DECOR_FLICKER_PHASE     = 2.4f;
	inline constexpr float CORE_FLICKER_DEPTH      = 0.1f;

	// --- Audio -----------------------------------------------------------------------
	inline constexpr float FOOTSTEP_INTERVAL       = 0.4f;
	inline constexpr float FOOTSTEP_MIN_SPEED      = 0.5f;
	inline constexpr float WARDEN_STEP_DISTANCE    = 0.7f;
	inline constexpr float LISTENER_HEIGHT         = 1.0f;

	inline constexpr float AUDIO_DRONE_VOLUME      = 0.45f;
	inline constexpr float AUDIO_CRACKLE_VOLUME    = 0.7f;
	inline constexpr float AUDIO_FOOTSTEP_VOLUME   = 0.7f;
	inline constexpr float AUDIO_WARDEN_STEP_VOLUME = 0.4f;
	inline constexpr float AUDIO_STRIKE_VOLUME     = 0.8f;
	inline constexpr float AUDIO_SNUFF_VOLUME      = 0.8f;
	inline constexpr float AUDIO_FLASK_VOLUME      = 0.9f;
	inline constexpr float AUDIO_ALERT_VOLUME      = 0.9f;
	inline constexpr float AUDIO_CAUGHT_VOLUME     = 1.0f;
	inline constexpr float AUDIO_IGNITE_VOLUME     = 0.9f;
	inline constexpr float AUDIO_WIN_VOLUME        = 1.0f;

	// Far is where a sound is silent, so GameAudio::PlayAt can skip an emitter already past it without losing anything audible.
	inline constexpr float AUDIO_CRACKLE_NEAR      = 1.5f;
	inline constexpr float AUDIO_CRACKLE_FAR       = 12.0f;
	inline constexpr float AUDIO_STEP_NEAR         = 1.5f;
	inline constexpr float AUDIO_STEP_FAR          = 10.0f;
	inline constexpr float AUDIO_WARDEN_NEAR       = 2.0f;
	inline constexpr float AUDIO_WARDEN_FAR        = 14.0f;
	inline constexpr float AUDIO_ALERT_NEAR        = 3.0f;
	inline constexpr float AUDIO_ALERT_FAR         = 20.0f;
	inline constexpr float AUDIO_IGNITE_NEAR       = 3.0f;
	inline constexpr float AUDIO_IGNITE_FAR        = 18.0f;

	// --- Colors ---------------------------------------------------------------------
	inline constexpr glm::vec4 COLOR_BG        = { 0.012f, 0.014f, 0.025f, 1.0f };
	inline constexpr glm::vec4 COLOR_STONE     = { 0.34f, 0.32f, 0.33f, 1.0f };
	inline constexpr glm::vec4 COLOR_FLOOR     = { 0.3f, 0.28f, 0.29f, 1.0f };
	inline constexpr glm::vec4 COLOR_BRASS     = { 0.72f, 0.55f, 0.24f, 1.0f };
	inline constexpr glm::vec4 COLOR_EMBER     = { 0.12f, 0.07f, 0.03f, 1.0f };
	inline constexpr glm::vec4 COLOR_ASH       = { 0.07f, 0.065f, 0.065f, 1.0f };
	inline constexpr glm::vec3 ASH_EMISSIVE_COLOR = { 1.0f, 0.45f, 0.15f };
	inline constexpr float ASH_EMISSIVE        = 0.04f;
	inline constexpr float ASH_ROUGHNESS       = 0.95f;
	inline constexpr glm::vec4 COLOR_WAX       = { 0.82f, 0.76f, 0.62f, 1.0f };
	inline constexpr float WAX_EMISSIVE        = 0.16f;
	inline constexpr float STONE_ROUGHNESS     = 0.9f;
	inline constexpr float CAP_ROUGHNESS       = 0.9f;
	inline constexpr float FLOOR_ROUGHNESS     = 0.8f;
	inline constexpr float BRASS_ROUGHNESS     = 0.55f;
	inline constexpr float BRASS_SPECULAR      = 0.12f;
	inline constexpr float WAX_ROUGHNESS       = 0.8f;
	inline constexpr glm::vec3 BRASS_EMISSIVE_COLOR = { 0.7f, 0.45f, 0.2f };
	inline constexpr float BRASS_EMISSIVE      = 0.18f;
	inline constexpr glm::vec4 COLOR_CAP       = { 0.2f, 0.22f, 0.28f, 1.0f };
	inline constexpr glm::vec3 CAP_EMISSIVE_COLOR = { 0.5f, 0.62f, 1.0f };
	inline constexpr float CAP_EMISSIVE        = 0.1f;
	inline constexpr glm::vec4 COLOR_CLOAK     = { 0.3f, 0.5f, 0.6f, 1.0f };
	inline constexpr glm::vec3 CLOAK_EMISSIVE_COLOR = { 0.35f, 0.55f, 0.7f };
	inline constexpr float CLOAK_EMISSIVE      = 0.14f;
	inline constexpr float CLOAK_ROUGHNESS     = 0.9f;
	inline constexpr glm::vec4 COLOR_SKIN      = { 0.82f, 0.66f, 0.54f, 1.0f };
	inline constexpr float FACE_EMISSIVE       = 0.22f;
	inline constexpr float FACE_ROUGHNESS      = 0.9f;

	inline constexpr glm::vec4 COLOR_GLASS     = { 1.0f, 0.82f, 0.55f, 1.0f };
	inline constexpr float GLASS_ROUGHNESS     = 0.2f;
	inline constexpr glm::vec4 COLOR_OIL       = { 0.95f, 0.72f, 0.2f, 1.0f };
	inline constexpr glm::vec3 OIL_EMISSIVE_COLOR = { 1.0f, 0.78f, 0.3f };
	inline constexpr float OIL_EMISSIVE        = 0.4f;
	inline constexpr float OIL_ROUGHNESS       = 0.4f;
	inline constexpr float OIL_SPECULAR        = 0.08f;

	inline constexpr glm::vec4 COLOR_ARMOUR        = { 0.36f, 0.4f, 0.48f, 1.0f };
	inline constexpr glm::vec3 ARMOUR_EMISSIVE_COLOR = { 0.45f, 0.55f, 0.8f };
	inline constexpr float ARMOUR_EMISSIVE         = 0.09f;
	inline constexpr float ARMOUR_ROUGHNESS        = 0.45f;
	inline constexpr float ARMOUR_SPECULAR         = 0.1f;
	inline constexpr glm::vec3 MARKER_CALM_COLOR       = { 0.35f, 0.6f, 1.0f };
	inline constexpr glm::vec3 MARKER_SUSPICIOUS_COLOR = { 1.0f, 0.72f, 0.18f };
	inline constexpr glm::vec3 MARKER_ALERT_COLOR      = { 1.0f, 0.16f, 0.1f };
	inline constexpr glm::vec4 COLOR_DEBUG_BASE    = { 0.0f, 0.0f, 0.0f, 1.0f };
	inline constexpr glm::vec3 DEBUG_CONE_COLOR    = { 0.3f, 0.95f, 1.0f };
	inline constexpr glm::vec3 DEBUG_DOT_COLOR     = { 0.35f, 1.0f, 0.3f };
	inline constexpr glm::vec3 DEBUG_SEEN_COLOR    = { 1.0f, 0.15f, 0.1f };
	inline constexpr glm::vec3 DEBUG_UNSEEN_COLOR  = { 0.6f, 0.6f, 0.65f };

	inline constexpr glm::vec4 COLOR_FADE          = { 0.0f, 0.0f, 0.0f, 1.0f };
	inline constexpr float HUD_FADE_Z              = 0.5f;

	inline constexpr glm::vec4 COLOR_METER_BG      = { 0.07f, 0.06f, 0.06f, 0.88f };
	inline constexpr glm::vec4 COLOR_METER_LIT     = { 1.0f, 0.7f, 0.26f, 1.0f };
	inline constexpr glm::vec4 COLOR_METER_SNUFFED = { 0.42f, 0.42f, 0.46f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_ALERT    = { 0.95f, 0.42f, 0.32f, 1.0f };
	inline constexpr glm::vec4 COLOR_PAUSE_DIM     = { 0.0f, 0.0f, 0.0f, 0.62f };

	inline constexpr glm::vec4 COLOR_TITLE     = { 1.0f, 0.74f, 0.42f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT      = { 0.92f, 0.9f, 0.84f, 1.0f };
	inline constexpr glm::vec4 COLOR_TEXT_DIM  = { 0.62f, 0.6f, 0.56f, 1.0f };
}
