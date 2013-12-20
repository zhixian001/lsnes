#include "lua/internal.hpp"
#include "library/framebuffer.hpp"

namespace
{
	struct render_object_pixel : public framebuffer::object
	{
		render_object_pixel(int32_t _x, int32_t _y, framebuffer::color _color) throw()
			: x(_x), y(_y), color(_color) {}
		~render_object_pixel() throw() {}
		template<bool X> void op(struct framebuffer::fb<X>& scr) throw()
		{
			color.set_palette(scr);
			int32_t _x = x + scr.get_origin_x();
			int32_t _y = y + scr.get_origin_y();
			if(_x < 0 || static_cast<uint32_t>(_x) >= scr.get_width())
				return;
			if(_y < 0 || static_cast<uint32_t>(_y) >= scr.get_height())
				return;
			color.apply(scr.rowptr(_y)[_x]);
		}
		void operator()(struct framebuffer::fb<true>& scr) throw()  { op(scr); }
		void operator()(struct framebuffer::fb<false>& scr) throw() { op(scr); }
		void clone(framebuffer::queue& q) const throw(std::bad_alloc) { q.clone_helper(this); }
	private:
		int32_t x;
		int32_t y;
		framebuffer::color color;
	};

	lua::fnptr gui_pixel(lua_func_misc, "gui.pixel", [](lua::state& L, const std::string& fname) -> int {
		if(!lua_render_ctx)
			return 0;
		int64_t color = 0xFFFFFFU;
		int32_t x = L.get_numeric_argument<int32_t>(1, fname.c_str());
		int32_t y = L.get_numeric_argument<int32_t>(2, fname.c_str());
		L.get_numeric_argument<int64_t>(3, color, fname.c_str());
		framebuffer::color pcolor(color);
		lua_render_ctx->queue->create_add<render_object_pixel>(x, y, pcolor);
		return 0;
	});
}
