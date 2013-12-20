#include "lua/internal.hpp"
#include "fonts/wrapper.hpp"
#include "library/framebuffer.hpp"

namespace
{
	struct render_object_text : public framebuffer::object
	{
		render_object_text(int32_t _x, int32_t _y, const std::string& _text, framebuffer::color _fg,
			framebuffer::color _bg, bool _hdbl = false, bool _vdbl = false) throw()
			: x(_x), y(_y), text(_text), fg(_fg), bg(_bg), hdbl(_hdbl), vdbl(_vdbl) {}
		~render_object_text() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			fg.set_palette(scr);
			bg.set_palette(scr);
			main_font.render(scr, x, y, text, fg, bg, hdbl, vdbl);
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		std::string text;
		framebuffer::color fg;
		framebuffer::color bg;
		bool hdbl;
		bool vdbl;
	};

	int internal_gui_text(lua::state& L, const std::string& fname, bool hdbl, bool vdbl)
	{
		if(!lua_render_ctx)
			return 0;
		int64_t fgc = 0xFFFFFFU;
		int64_t bgc = -1;
		int32_t _x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t _y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		L.get_numeric_argument<int64_t>(4, fgc, fname.c_str());
		L.get_numeric_argument<int64_t>(5, bgc, fname.c_str());
		std::string text = L.get_string(3, fname.c_str());
		framebuffer::color fg(fgc);
		framebuffer::color bg(bgc);
		lua_render_ctx->queue->create_add<render_object_text>(_x, _y, text, fg, bg, hdbl, vdbl);
		return 0;
	}

	lua::fnptr gui_text(lua_func_misc, "gui.text", [](lua::state& L, const std::string& fname) -> int {
		return internal_gui_text(L, fname, false, false);
	});

	lua::fnptr gui_textH(lua_func_misc, "gui.textH", [](lua::state& L, const std::string& fname) -> int {
		return internal_gui_text(L, fname, true, false);
	});

	lua::fnptr gui_textV(lua_func_misc, "gui.textV", [](lua::state& L, const std::string& fname) -> int {
		return internal_gui_text(L, fname, false, true);
	});

	lua::fnptr gui_textHV(lua_func_misc, "gui.textHV", [](lua::state& L, const std::string& fname)
		-> int {
		return internal_gui_text(L, fname, true, true);
	});
}
