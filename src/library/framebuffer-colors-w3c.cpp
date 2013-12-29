#include "framebuffer.hpp"

namespace
{
	framebuffer::basecolor aliceblue("aliceblue", 0xf0f8ff);
	framebuffer::basecolor antiquewhite("antiquewhite", 0xfaebd7);
	framebuffer::basecolor aqua("aqua", 0x00ffff);
	framebuffer::basecolor aquamarine("aquamarine", 0x7fffd4);
	framebuffer::basecolor azure("azure", 0xf0ffff);
	framebuffer::basecolor beige("beige", 0xf5f5dc);
	framebuffer::basecolor bisque("bisque", 0xffe4c4);
	framebuffer::basecolor black("black", 0x000000);
	framebuffer::basecolor blanchedalmond("blanchedalmond", 0xffebcd);
	framebuffer::basecolor blue("blue", 0x0000ff);
	framebuffer::basecolor blueviolet("blueviolet", 0x8a2be2);
	framebuffer::basecolor brown("brown", 0xa52a2a);
	framebuffer::basecolor burlywood("burlywood", 0xdeb887);
	framebuffer::basecolor cadetblue("cadetblue", 0x5f9ea0);
	framebuffer::basecolor chartreuse("chartreuse", 0x7fff00);
	framebuffer::basecolor chocolate("chocolate", 0xd2691e);
	framebuffer::basecolor coral("coral", 0xff7f50);
	framebuffer::basecolor cornflowerblue("cornflowerblue", 0x6495ed);
	framebuffer::basecolor cornsilk("cornsilk", 0xfff8dc);
	framebuffer::basecolor crimson("crimson", 0xdc143c);
	framebuffer::basecolor cyan("cyan", 0x00ffff);
	framebuffer::basecolor darkblue("darkblue", 0x00008b);
	framebuffer::basecolor darkcyan("darkcyan", 0x008b8b);
	framebuffer::basecolor darkgoldenrod("darkgoldenrod", 0xb8860b);
	framebuffer::basecolor darkgray("darkgray", 0xa9a9a9);
	framebuffer::basecolor darkgreen("darkgreen", 0x006400);
	framebuffer::basecolor darkgrey("darkgrey", 0xa9a9a9);
	framebuffer::basecolor darkkhaki("darkkhaki", 0xbdb76b);
	framebuffer::basecolor darkmagenta("darkmagenta", 0x8b008b);
	framebuffer::basecolor darkolivegreen("darkolivegreen", 0x556b2f);
	framebuffer::basecolor darkorange("darkorange", 0xff8c00);
	framebuffer::basecolor darkorchid("darkorchid", 0x9932cc);
	framebuffer::basecolor darkred("darkred", 0x8b0000);
	framebuffer::basecolor darksalmon("darksalmon", 0xe9967a);
	framebuffer::basecolor darkseagreen("darkseagreen", 0x8fbc8f);
	framebuffer::basecolor darkslateblue("darkslateblue", 0x483d8b);
	framebuffer::basecolor darkslategray("darkslategray", 0x2f4f4f);
	framebuffer::basecolor darkslategrey("darkslategrey", 0x2f4f4f);
	framebuffer::basecolor darkturquoise("darkturquoise", 0x00ced1);
	framebuffer::basecolor darkviolet("darkviolet", 0x9400d3);
	framebuffer::basecolor deeppink("deeppink", 0xff1493);
	framebuffer::basecolor deepskyblue("deepskyblue", 0x00bfff);
	framebuffer::basecolor dimgray("dimgray", 0x696969);
	framebuffer::basecolor dimgrey("dimgrey", 0x696969);
	framebuffer::basecolor dodgerblue("dodgerblue", 0x1e90ff);
	framebuffer::basecolor firebrick("firebrick", 0xb22222);
	framebuffer::basecolor floralwhite("floralwhite", 0xfffaf0);
	framebuffer::basecolor forestgreen("forestgreen", 0x228b22);
	framebuffer::basecolor fuchsia("fuchsia", 0xff00ff);
	framebuffer::basecolor gainsboro("gainsboro", 0xdcdcdc);
	framebuffer::basecolor ghostwhite("ghostwhite", 0xf8f8ff);
	framebuffer::basecolor gold("gold", 0xffd700);
	framebuffer::basecolor goldenrod("goldenrod", 0xdaa520);
	framebuffer::basecolor gray("gray", 0x808080);
	framebuffer::basecolor green("green", 0x008000);
	framebuffer::basecolor greenyellow("greenyellow", 0xadff2f);
	framebuffer::basecolor grey("grey", 0x808080);
	framebuffer::basecolor honeydew("honeydew", 0xf0fff0);
	framebuffer::basecolor hotpink("hotpink", 0xff69b4);
	framebuffer::basecolor indianred("indianred", 0xcd5c5c);
	framebuffer::basecolor indigo("indigo", 0x4b0082);
	framebuffer::basecolor ivory("ivory", 0xfffff0);
	framebuffer::basecolor khaki("khaki", 0xf0e68c);
	framebuffer::basecolor lavender("lavender", 0xe6e6fa);
	framebuffer::basecolor lavenderblush("lavenderblush", 0xfff0f5);
	framebuffer::basecolor lawngreen("lawngreen", 0x7cfc00);
	framebuffer::basecolor lemonchiffon("lemonchiffon", 0xfffacd);
	framebuffer::basecolor lightblue("lightblue", 0xadd8e6);
	framebuffer::basecolor lightcoral("lightcoral", 0xf08080);
	framebuffer::basecolor lightcyan("lightcyan", 0xe0ffff);
	framebuffer::basecolor lightgoldenrodyellow("lightgoldenrodyellow", 0xfafad2);
	framebuffer::basecolor lightgray("lightgray", 0xd3d3d3);
	framebuffer::basecolor lightgreen("lightgreen", 0x90ee90);
	framebuffer::basecolor lightgrey("lightgrey", 0xd3d3d3);
	framebuffer::basecolor lightpink("lightpink", 0xffb6c1);
	framebuffer::basecolor lightsalmon("lightsalmon", 0xffa07a);
	framebuffer::basecolor lightseagreen("lightseagreen", 0x20b2aa);
	framebuffer::basecolor lightskyblue("lightskyblue", 0x87cefa);
	framebuffer::basecolor lightslategray("lightslategray", 0x778899);
	framebuffer::basecolor lightslategrey("lightslategrey", 0x778899);
	framebuffer::basecolor lightsteelblue("lightsteelblue", 0xb0c4de);
	framebuffer::basecolor lightyellow("lightyellow", 0xffffe0);
	framebuffer::basecolor lime("lime", 0x00ff00);
	framebuffer::basecolor limegreen("limegreen", 0x32cd32);
	framebuffer::basecolor linen("linen", 0xfaf0e6);
	framebuffer::basecolor magenta("magenta", 0xff00ff);
	framebuffer::basecolor maroon("maroon", 0x800000);
	framebuffer::basecolor mediumaquamarine("mediumaquamarine", 0x66cdaa);
	framebuffer::basecolor mediumblue("mediumblue", 0x0000cd);
	framebuffer::basecolor mediumorchid("mediumorchid", 0xba55d3);
	framebuffer::basecolor mediumpurple("mediumpurple", 0x9370db);
	framebuffer::basecolor mediumseagreen("mediumseagreen", 0x3cb371);
	framebuffer::basecolor mediumslateblue("mediumslateblue", 0x7b68ee);
	framebuffer::basecolor mediumspringgreen("mediumspringgreen", 0x00fa9a);
	framebuffer::basecolor mediumturquoise("mediumturquoise", 0x48d1cc);
	framebuffer::basecolor mediumvioletred("mediumvioletred", 0xc71585);
	framebuffer::basecolor midnightblue("midnightblue", 0x191970);
	framebuffer::basecolor mintcream("mintcream", 0xf5fffa);
	framebuffer::basecolor mistyrose("mistyrose", 0xffe4e1);
	framebuffer::basecolor moccasin("moccasin", 0xffe4b5);
	framebuffer::basecolor navajowhite("navajowhite", 0xffdead);
	framebuffer::basecolor navy("navy", 0x000080);
	framebuffer::basecolor oldlace("oldlace", 0xfdf5e6);
	framebuffer::basecolor olive("olive", 0x808000);
	framebuffer::basecolor olivedrab("olivedrab", 0x6b8e23);
	framebuffer::basecolor orange("orange", 0xffa500);
	framebuffer::basecolor orangered("orangered", 0xff4500);
	framebuffer::basecolor orchid("orchid", 0xda70d6);
	framebuffer::basecolor palegoldenrod("palegoldenrod", 0xeee8aa);
	framebuffer::basecolor palegreen("palegreen", 0x98fb98);
	framebuffer::basecolor paleturquoise("paleturquoise", 0xafeeee);
	framebuffer::basecolor palevioletred("palevioletred", 0xdb7093);
	framebuffer::basecolor papayawhip("papayawhip", 0xffefd5);
	framebuffer::basecolor peachpuff("peachpuff", 0xffdab9);
	framebuffer::basecolor peru("peru", 0xcd853f);
	framebuffer::basecolor pink("pink", 0xffc0cb);
	framebuffer::basecolor plum("plum", 0xdda0dd);
	framebuffer::basecolor powderblue("powderblue", 0xb0e0e6);
	framebuffer::basecolor purple("purple", 0x800080);
	framebuffer::basecolor red("red", 0xff0000);
	framebuffer::basecolor rosybrown("rosybrown", 0xbc8f8f);
	framebuffer::basecolor royalblue("royalblue", 0x4169e1);
	framebuffer::basecolor saddlebrown("saddlebrown", 0x8b4513);
	framebuffer::basecolor salmon("salmon", 0xfa8072);
	framebuffer::basecolor sandybrown("sandybrown", 0xf4a460);
	framebuffer::basecolor seagreen("seagreen", 0x2e8b57);
	framebuffer::basecolor seashell("seashell", 0xfff5ee);
	framebuffer::basecolor sienna("sienna", 0xa0522d);
	framebuffer::basecolor silver("silver", 0xc0c0c0);
	framebuffer::basecolor skyblue("skyblue", 0x87ceeb);
	framebuffer::basecolor slateblue("slateblue", 0x6a5acd);
	framebuffer::basecolor slategray("slategray", 0x708090);
	framebuffer::basecolor slategrey("slategrey", 0x708090);
	framebuffer::basecolor snow("snow", 0xfffafa);
	framebuffer::basecolor springgreen("springgreen", 0x00ff7f);
	framebuffer::basecolor steelblue("steelblue", 0x4682b4);
	framebuffer::basecolor tan("tan", 0xd2b48c);
	framebuffer::basecolor teal("teal", 0x008080);
	framebuffer::basecolor thistle("thistle", 0xd8bfd8);
	framebuffer::basecolor tomato("tomato", 0xff6347);
	framebuffer::basecolor turquoise("turquoise", 0x40e0d0);
	framebuffer::basecolor violet("violet", 0xee82ee);
	framebuffer::basecolor wheat("wheat", 0xf5deb3);
	framebuffer::basecolor white("white", 0xffffff);
	framebuffer::basecolor whitesmoke("whitesmoke", 0xf5f5f5);
	framebuffer::basecolor yellow("yellow", 0xffff00);
	framebuffer::basecolor yellowgreen("yellowgreen", 0x9acd32);
}