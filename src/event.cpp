
//////
//
// Includes
//

// SDL3 library
#include <SDL3/SDL.h>

// Local includes
#include "FCG/event.h"



//////
//
// Module namespace open
//

/// The library top-level namespace.
namespace fcg {



//////
//
// Functions
//

auto makeEvent (const SDL_Event &e) -> Event
{
	switch (e.type)
	{
		case SDL_EVENT_KEY_DOWN:
			return {EventType::KeyDown, &e, KeyEvent{e.key.key, (uint32_t)e.key.scancode, e.key.mod, e.key.repeat}};

		case SDL_EVENT_KEY_UP:
			return {EventType::KeyUp, &e, KeyEvent{e.key.key, (uint32_t)e.key.scancode, e.key.mod, false}};

		case SDL_EVENT_TEXT_INPUT:
			return {EventType::TextInput, &e, TextEvent{e.text.text ? e.text.text : ""}};

		case SDL_EVENT_TEXT_EDITING:
			return {EventType::TextEditing, &e, TextEvent{e.edit.text ? e.edit.text : ""}};

		case SDL_EVENT_MOUSE_MOTION:
			return {
				EventType::MouseMotion, &e,
				MouseMotionEvent{e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel, e.motion.state}
			};

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			return {
				EventType::MouseButtonDown, &e,
				MouseButtonEvent{
					e.button.x, e.button.y, static_cast<MouseButton>(e.button.button), e.button.clicks
				}
			};

		case SDL_EVENT_MOUSE_BUTTON_UP:
			return {
				EventType::MouseButtonUp, &e,
				MouseButtonEvent{
					e.button.x, e.button.y, static_cast<MouseButton>(e.button.button), e.button.clicks
				}
			};

		case SDL_EVENT_MOUSE_WHEEL:
			return {EventType::MouseWheel, &e, MouseWheelEvent{e.wheel.x, e.wheel.y}};

		case SDL_EVENT_DROP_BEGIN:
			return {EventType::DropBegin, &e, DropEvent{}};

		case SDL_EVENT_DROP_POSITION:
			return {EventType::DropPosition, &e, DropEvent{e.drop.x, e.drop.y, {}}};

		case SDL_EVENT_DROP_FILE:
			return {EventType::DropFile, &e, DropEvent{e.drop.x, e.drop.y, e.drop.data ? e.drop.data : ""}};

		case SDL_EVENT_DROP_TEXT:
			return {EventType::DropText, &e, DropEvent{e.drop.x, e.drop.y, e.drop.data ? e.drop.data : ""}};

		case SDL_EVENT_DROP_COMPLETE:
			return {EventType::DropComplete, &e, DropEvent{}};

		default:
			return {EventType::Raw, &e};
	}
}



//////
//
// Module namespace close
//

} // namespace fcg
