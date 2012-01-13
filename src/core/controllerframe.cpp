#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/misc.hpp"

#include <cstdio>
#include <iostream>

#define SYSTEM_BYTES 5

namespace
{
	std::set<porttype_info*>& porttypes()
	{
		static std::set<porttype_info*> p;
		return p;
	}

	const char* buttonnames[MAX_LOGICAL_BUTTONS] = {
		"left", "right", "up", "down", "A", "B", "X", "Y", "L", "R", "select", "start", "trigger",
		"cursor", "turbo", "pause"
	};
}

/**
 * Get name of logical button.
 *
 * Parameter lbid: ID of logical button.
 * Returns: The name of button.
 * Throws std::bad_alloc: Not enough memory.
 */
std::string get_logical_button_name(unsigned lbid) throw(std::bad_alloc)
{
	if(lbid >= MAX_LOGICAL_BUTTONS)
		return "";
	return buttonnames[lbid];
}

const porttype_info& porttype_info::lookup(porttype_t p) throw(std::runtime_error)
{
	for(auto i : porttypes())
		if(p == i->value)
			return *i;
	throw std::runtime_error("Bad port type");
}

const porttype_info& porttype_info::lookup(const std::string& p) throw(std::runtime_error)
{
	for(auto i : porttypes())
		if(p == i->name)
			return *i;
	throw std::runtime_error("Bad port type");
}

porttype_info::~porttype_info() throw()
{
	porttypes().erase(this);
}

porttype_info::porttype_info(porttype_t ptype, const std::string& pname, size_t psize) throw(std::bad_alloc)
{
	value = ptype;
	name = pname;
	storage_size = psize;
	porttypes().insert(this);
}

pollcounter_vector::pollcounter_vector() throw()
{
	clear();
}

void pollcounter_vector::clear() throw()
{
	system_flag = false;
	memset(ctrs, 0, sizeof(ctrs));
}

void pollcounter_vector::set_all_DRDY() throw()
{
	for(size_t i = 0; i < MAX_BUTTONS ; i++)
		ctrs[i] |= 0x80000000UL;
}

#define INDEXOF(pcid, ctrl) ((pcid) * MAX_CONTROLS_PER_CONTROLLER + (ctrl))

void pollcounter_vector::clear_DRDY(unsigned pcid, unsigned ctrl) throw()
{
	ctrs[INDEXOF(pcid, ctrl)] &= 0x7FFFFFFFUL;
}

bool pollcounter_vector::get_DRDY(unsigned pcid, unsigned ctrl) throw()
{
	return ((ctrs[INDEXOF(pcid, ctrl)] & 0x80000000UL) != 0);
}

bool pollcounter_vector::has_polled() throw()
{
	uint32_t res = system_flag ? 1 : 0;
	for(size_t i = 0; i < MAX_BUTTONS ; i++)
		res |= ctrs[i];
	return ((res & 0x7FFFFFFFUL) != 0);
}

uint32_t pollcounter_vector::get_polls(unsigned pcid, unsigned ctrl) throw()
{
	return ctrs[INDEXOF(pcid, ctrl)] & 0x7FFFFFFFUL;
}

uint32_t pollcounter_vector::increment_polls(unsigned pcid, unsigned ctrl) throw()
{
	size_t i = INDEXOF(pcid, ctrl);
	uint32_t x = ctrs[i] & 0x7FFFFFFFUL;
	++ctrs[i];
	return x;
}

void pollcounter_vector::set_system() throw()
{
	system_flag = true;
}

bool pollcounter_vector::get_system() throw()
{
	return system_flag;
}

uint32_t pollcounter_vector::max_polls() throw()
{
	uint32_t max = system_flag ? 1 : 0;
	for(unsigned i = 0; i < MAX_BUTTONS; i++) {
		uint32_t tmp = ctrs[i] & 0x7FFFFFFFUL;
		max = (max < tmp) ? tmp : max;
	}
	return max;
}

void pollcounter_vector::save_state(std::vector<uint32_t>& mem) throw(std::bad_alloc)
{
	mem.resize(4 + MAX_BUTTONS );
	//Compatiblity fun.
	mem[0] = 0x80000000UL;
	mem[1] = system_flag ? 1 : 0x80000000UL;
	mem[2] = system_flag ? 1 : 0x80000000UL;
	mem[3] = system_flag ? 1 : 0x80000000UL;
	for(size_t i = 0; i < MAX_BUTTONS ; i++)
		mem[4 + i] = ctrs[i];
}

void pollcounter_vector::load_state(const std::vector<uint32_t>& mem) throw()
{
	system_flag = (mem[1] | mem[2] | mem[3]) & 0x7FFFFFFFUL;
	for(size_t i = 0; i < MAX_BUTTONS ; i++)
		ctrs[i] = mem[i + 4];
}

bool pollcounter_vector::check(const std::vector<uint32_t>& mem) throw()
{
	return (mem.size() == MAX_BUTTONS  + 4);
}


controller_frame::controller_frame(porttype_t p1, porttype_t p2) throw(std::runtime_error)
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	types[0] = p1;
	types[1] = p2;
	set_types(types);
}

controller_frame::controller_frame(unsigned char* mem, porttype_t p1, porttype_t p2) throw(std::runtime_error)
{
	if(!mem)
		throw std::runtime_error("NULL backing memory not allowed");
	memset(memory, 0, sizeof(memory));
	backing = mem;
	types[0] = p1;
	types[1] = p2;
	set_types(types);
}

controller_frame::controller_frame(const controller_frame& obj) throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	set_types(obj.types);
	memcpy(backing, obj.backing, totalsize);
}

controller_frame& controller_frame::operator=(const controller_frame& obj) throw(std::runtime_error)
{
	set_types(obj.types);
	memcpy(backing, obj.backing, totalsize);
}

void controller_frame::set_types(const porttype_t* tarr)
{
	for(unsigned i = 0; i < MAX_PORTS; i++) {
		if(memory != backing && types[i] != tarr[i])
			throw std::runtime_error("Controller_frame: Type mismatch");
		if(!porttype_info::lookup(tarr[i]).legal(i))
			throw std::runtime_error("Illegal port type for port index");
	}
	size_t offset = SYSTEM_BYTES;
	for(unsigned i = 0; i < MAX_PORTS; i++) {
		offsets[i] = offset;
		types[i] = tarr[i];
		pinfo[i] = &porttype_info::lookup(tarr[i]);
		offset += pinfo[i]->storage_size;
	}
	totalsize = offset;
}

size_t controller_frame_vector::walk_helper(size_t frame, bool sflag) throw()
{
	size_t ret = sflag ? frame : 0;
	if(frame >= frames)
		return ret;
	frame++;
	ret++;
	size_t page = frame / frames_per_page;
	size_t offset = frame_size * (frame % frames_per_page);
	size_t index = frame % frames_per_page;
	if(cache_page_num != page) {
		cache_page = &pages[page];
		cache_page_num = page;
	}
	while(frame < frames) {
		if(index == frames_per_page) {
			page++;
			cache_page = &pages[page];
			cache_page_num = page;
		}
		if(controller_frame::sync(cache_page->content + offset))
			break;
		index++;
		offset += frame_size;
		frame++;
		ret++;
	}
	return ret;
}

size_t controller_frame_vector::count_frames() throw()
{
	size_t ret = 0;
	if(!frames)
		return 0;
	cache_page_num = 0;
	cache_page = &pages[0];
	size_t offset = 0;
	size_t index = 0;
	for(size_t i = 0; i < frames; i++) {
		if(index == frames_per_page) {
			cache_page_num++;
			cache_page = &pages[cache_page_num];
			index = 0;
			offset = 0;
		}
		if(controller_frame::sync(cache_page->content + offset))
			ret++;
		index++;
		offset += frame_size;
		
	}
	return ret;
}

void controller_frame_vector::clear(enum porttype_t p1, enum porttype_t p2) throw(std::runtime_error)
{
	controller_frame check(p1, p2);
	frame_size = check.size();
	frames_per_page = CONTROLLER_PAGE_SIZE / frame_size;
	frames = 0;
	types[0] = p1;
	types[1] = p2;
	clear_cache();
	pages.clear();
}

controller_frame_vector::~controller_frame_vector() throw()
{
	pages.clear();
	cache_page = NULL;
}

controller_frame_vector::controller_frame_vector(enum porttype_t p1, enum porttype_t p2) throw(std::runtime_error)
{
	clear(p1, p2);
}

void controller_frame_vector::append(controller_frame frame) throw(std::bad_alloc, std::runtime_error)
{
	controller_frame check(types[0], types[1]);
	if(!check.types_match(frame))
		throw std::runtime_error("controller_frame_vector::append: Type mismatch");
	if(frames % frames_per_page == 0) {
		//Create new page.
		pages[frames / frames_per_page];
	}
	//Write the entry.
	size_t page = frames / frames_per_page;
	size_t offset = frame_size * (frames % frames_per_page);
	if(cache_page_num != page) {
		cache_page_num = page;
		cache_page = &pages[page];
	}
	controller_frame(cache_page->content + offset, types[0], types[1]) = frame;
	frames++;
}

controller_frame_vector::controller_frame_vector(const controller_frame_vector& vector) throw(std::bad_alloc)
{
	clear(vector.types[0], vector.types[1]);
	*this = vector;
}

controller_frame_vector& controller_frame_vector::operator=(const controller_frame_vector& v)
	throw(std::bad_alloc)
{
	if(this == &v)
		return *this;
	resize(v.frames);
	clear_cache();

	//Copy the fields.
	frame_size = v.frame_size;
	frames_per_page = v.frames_per_page;
	for(size_t i = 0; i < MAX_PORTS; i++)
		types[i] = v.types[i];

	//This can't fail anymore. Copy the raw page contents.
	size_t pagecount = (frames + frames_per_page - 1) / frames_per_page;
	for(size_t i = 0; i < pagecount; i++) {
		page& pg = pages[i];
		const page& pg2 = v.pages.find(i)->second;
		pg = pg2;
	}

	return *this;
}

size_t controller_frame::system_serialize(const unsigned char* buffer, char* textbuf)
{
	char tmp[128];
	if(buffer[1] || buffer[2] || buffer[3] || buffer[4])
		sprintf(tmp, "%c%c %i %i", ((buffer[0] & 1) ? 'F' : '.'), ((buffer[0] & 2) ? 'R' : '.'),
			unserialize_short(buffer + 1), unserialize_short(buffer + 3));
	else
		sprintf(tmp, "%c%c", ((buffer[0] & 1) ? 'F' : '.'), ((buffer[0] & 2) ? 'R' : '.'));
	size_t len = strlen(tmp);
	memcpy(textbuf, tmp, len);
	return len;
}

size_t controller_frame::system_deserialize(unsigned char* buffer, const char* textbuf)
{
	buffer[0] = 0;
	size_t idx = 0;
	if(read_button_value(textbuf, idx))
		buffer[0] |= 1;
	if(read_button_value(textbuf, idx))
		buffer[0] |= 2;
	serialize_short(buffer + 1, read_axis_value(textbuf, idx));
	serialize_short(buffer + 3, read_axis_value(textbuf, idx));
	skip_rest_of_field(textbuf, idx, false);
	return idx;
}

short read_axis_value(const char* buf, size_t& idx) throw()
{
		char ch;
		//Skip ws.
		while(is_nonterminator(buf[idx])) {
			char ch = buf[idx];
			if(ch != ' ' && ch != '\t')
				break;
			idx++;
		}
		//Read the sign if any.
		if(!is_nonterminator(buf[idx]))
			return 0;
		bool negative = false;
		if(ch == '-') {
			negative = true;
			idx++;
		}
		if(ch == '+')
			idx++;

		//Read numeric value.
		int numval = 0;
		while(!is_nonterminator(buf[idx]) && isdigit(static_cast<unsigned char>(ch = buf[idx]))) {
			numval = numval * 10 + (ch - '0');
			idx++;
		}
		if(negative)
			numval = -numval;

		return static_cast<short>(numval);
}

void controller_frame_vector::resize(size_t newsize) throw(std::bad_alloc)
{
	clear_cache();
	if(newsize == 0) {
		clear();
	} else if(newsize < frames) {
		//Shrink movie.
		size_t current_pages = (frames + frames_per_page - 1) / frames_per_page;
		size_t pages_needed = (newsize + frames_per_page - 1) / frames_per_page;
		for(size_t i = pages_needed; i < current_pages; i++)
			pages.erase(i);
		//Now zeroize the excess memory.
		size_t offset = frame_size * (newsize % frames_per_page);
		memset(pages[pages_needed - 1].content + offset, 0, CONTROLLER_PAGE_SIZE - offset);
		frames = newsize;
	} else if(newsize > frames) {
		//Enlarge movie.
		size_t current_pages = (frames + frames_per_page - 1) / frames_per_page;
		size_t pages_needed = (newsize + frames_per_page - 1) / frames_per_page;
		//Create the needed pages.
		for(size_t i = current_pages; i < pages_needed; i++) {
			try {
				pages[i];
			} catch(...) {
				for(size_t i = current_pages; i < pages_needed; i++)
					if(pages.count(i))
						pages.erase(i);
				throw;
			}
		}
		frames = newsize;
	}
}

controller_frame::controller_frame() throw()
{
	memset(memory, 0, sizeof(memory));
	backing = memory;
	for(unsigned i = 0; i < MAX_PORTS; i++) {
		offsets[i] = SYSTEM_BYTES;
		types[i] = PT_INVALID;
		pinfo[i] = NULL;
	}
	totalsize = SYSTEM_BYTES;
}

void controller_frame::set_port_type(unsigned port, porttype_t ptype) throw(std::runtime_error)
{
	char tmp[MAXIMUM_CONTROLLER_FRAME_SIZE] = {0};
	if(!porttype_info::lookup(ptype).legal(port))
		throw std::runtime_error("Illegal port type for port index");
	if(memory != backing)
		throw std::runtime_error("Can't set port type on non-dedicated controller frame");
	if(port >= MAX_PORTS)
		return;
	const porttype_info* newpinfo[MAX_PORTS];
	size_t newoffsets[MAX_PORTS];
	size_t offset = SYSTEM_BYTES;
	for(size_t i = 0; i < MAX_PORTS; i++) {
		if(i != port)
			newpinfo[i] = pinfo[i];
		else
			newpinfo[i] = &porttype_info::lookup(ptype);
		newoffsets[i] = offset;
		if(newpinfo[i])
			offset += newpinfo[i]->storage_size;
		if(i != port && newpinfo[i] && newpinfo[i]->storage_size)
			memcpy(tmp + newoffsets[i], backing + offsets[i], newpinfo[i]->storage_size);
	}
	memcpy(memory, tmp, MAXIMUM_CONTROLLER_FRAME_SIZE);
	types[port] = ptype;
	pinfo[port] = newpinfo[port];
	for(size_t i = 0; i < MAX_PORTS; i++)
		offsets[i] = newoffsets[i];
	totalsize = offset;
}

controller_state::controller_state() throw()
{
	for(size_t i = 0; i < MAX_ANALOG; i++) {
		analog_indices[i] = -1;
		analog_mouse[i] = false;
	}
	for(size_t i = 0; i < MAX_PORTS; i++) {
		porttypes[i] = PT_INVALID;
		porttypeinfo[i] = NULL;
	}
}

int controller_state::lcid_to_pcid(unsigned lcid) throw()
{
	if(!porttypeinfo[0] || !porttypeinfo[1])
		return -1;
	unsigned p1devs = porttypeinfo[0]->controllers();
	unsigned p2devs = porttypeinfo[1]->controllers();
	if(lcid >= p1devs + p2devs)
		return -1;
	//Exceptional: If p1 is none, map all to p2.
	if(!p1devs)
		return lcid + MAX_CONTROLLERS_PER_PORT;
	//LID 0 Is always PID 0 unless out of range.
	if(lcid == 0)
		return 0;
	//LID 1-n are on port 2.
	else if(lcid < 1 + p2devs)
		return lcid - 1 + MAX_CONTROLLERS_PER_PORT;
	//From there, those are on port 1 (except for the first).
	else
		return lcid - p2devs;
}

int controller_state::acid_to_pcid(unsigned acid) throw()
{
	if(acid > MAX_ANALOG)
		return -1;
	return analog_indices[acid];
}

bool controller_state::acid_is_mouse(unsigned acid) throw()
{
	if(acid > MAX_ANALOG)
		return -1;
	return analog_mouse[acid];
	
}

devicetype_t controller_state::pcid_to_type(unsigned pcid) throw()
{
	size_t port = pcid / MAX_CONTROLLERS_PER_PORT;
	if(port >= MAX_PORTS)
		return DT_NONE;
	return porttypeinfo[port]->devicetype(pcid % MAX_CONTROLLERS_PER_PORT);
}

controller_frame controller_state::get(uint64_t framenum) throw()
{
	if(_autofire.size())
		return _input ^ _autohold ^ _autofire[framenum % _autofire.size()];
	else
		return _input ^ _autohold;
}

void controller_state::analog(unsigned acid, int x, int y) throw()
{
	if(acid >= MAX_ANALOG || analog_indices[acid] < 0) {
		messages << "No analog controller #" << acid << std::endl;
		return;
	}
	_input.axis(analog_indices[acid], 0, x);
	_input.axis(analog_indices[acid], 1, y);
}

void controller_state::reset(int32_t delay) throw()
{
	if(delay >= 0) {
		_input.reset(true);
		_input.delay(std::make_pair(delay / 10000, delay % 10000));
	} else {
		_input.reset(false);
		_input.delay(std::make_pair(0, 0));
	}
}

void controller_state::autohold(unsigned pcid, unsigned pbid, bool newstate) throw()
{
	_autohold.axis(pcid, pbid, newstate ? 1 : 0);
	information_dispatch::do_autohold_update(pcid, pbid, newstate);
}

bool controller_state::autohold(unsigned pcid, unsigned pbid) throw()
{
	return (_autohold.axis(pcid, pbid) != 0);
}

void controller_state::button(unsigned pcid, unsigned pbid, bool newstate) throw()
{
	_input.axis(pcid, pbid, newstate ? 1 : 0);
}

bool controller_state::button(unsigned pcid, unsigned pbid) throw()
{
	return (_input.axis(pcid, pbid) != 0);
}

void controller_state::autofire(std::vector<controller_frame> pattern) throw(std::bad_alloc)
{
	_autofire = pattern;
}

int controller_state::button_id(unsigned pcid, unsigned lbid) throw()
{
	size_t port = pcid / MAX_CONTROLLERS_PER_PORT;
	if(port >= MAX_PORTS)
		return -1;
	return porttypeinfo[port]->button_id(pcid % MAX_CONTROLLERS_PER_PORT, lbid);
}

void controller_state::set_port(unsigned port, porttype_t ptype, bool set_core) throw(std::runtime_error)
{
	if(port >= MAX_PORTS)
		throw std::runtime_error("Port number invalid");
	const porttype_info* info = &porttype_info::lookup(ptype);
	if(!info->legal(port))
		throw std::runtime_error("Port type not valid for port");
	if(set_core)
		info->set_core_controller(port);
	porttype_t oldtype = porttypes[port];
	if(oldtype != ptype) {
		_input.set_port_type(port, ptype);
		_autohold.set_port_type(port, ptype);
		_committed.set_port_type(port, ptype);
		//The old autofire pattern no longer applies.
		_autofire.clear();
	}
	porttypes[port] = ptype;
	porttypeinfo[port] = info;
	int i = 0;
	for(unsigned j = 0; j < MAX_ANALOG; j++)
		analog_indices[j] = -1;
	for(unsigned j = 0; j < MAX_PORTS * MAX_CONTROLLERS_PER_PORT; j++) {
		if(!porttypeinfo[j / MAX_CONTROLLERS_PER_PORT])
			continue;
		devicetype_t d = porttypeinfo[j / MAX_CONTROLLERS_PER_PORT]->devicetype(j % MAX_CONTROLLERS_PER_PORT);
		switch(d) {
		case DT_NONE:
		case DT_GAMEPAD:
			break;
		case DT_MOUSE:
			analog_mouse[i] = true;
			analog_indices[i++] = j;
			break;
		case DT_SUPERSCOPE:
		case DT_JUSTIFIER:
			analog_mouse[i] = false;
			analog_indices[i++] = j;
			break;
		}
		if(i == MAX_ANALOG)
			break;
	}
	information_dispatch::do_autohold_reconfigure();
}

controller_frame controller_state::get_blank() throw()
{
	return _input.blank_frame();
}

std::string controller_state::lcid_to_typestring(unsigned lcid) throw(std::bad_alloc)
{
	int pcid = lcid_to_pcid(lcid);
	devicetype_t dtype = DT_NONE;
	if(pcid >= 0)
		dtype = pcid_to_type(pcid);
	switch(dtype) {
	case DT_NONE:			return "disconnected";
	case DT_GAMEPAD:		return "gamepad";
	case DT_MOUSE:			return "mouse";
	case DT_SUPERSCOPE:		return "superscope";
	case DT_JUSTIFIER:		return "justifier";
	default:			return "unknown";
	};
}

controller_frame controller_state::commit(uint64_t framenum) throw()
{
	controller_frame f = get(framenum);
	_committed = f;
	return _committed;
}

controller_frame controller_state::get_committed() throw()
{
	return _committed;
}

controller_frame controller_state::commit(controller_frame controls) throw()
{
	_committed = controls;
	return _committed;
}