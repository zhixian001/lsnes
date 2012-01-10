#ifndef _plat_wxwidgets__window_mainwindow__hpp__included__
#define _plat_wxwidgets__window_mainwindow__hpp__included__

#include <stack>

#include <wx/string.h>
#include <wx/wx.h>

class wxwin_mainwindow : public wxFrame
{
public:
	class panel : public wxPanel
	{
	public:
		panel(wxWindow* win);
		void on_paint(wxPaintEvent& e);
		void request_paint();
		void on_erase(wxEraseEvent& e);
		void on_keyboard_down(wxKeyEvent& e);
		void on_keyboard_up(wxKeyEvent& e);
		void on_mouse(wxMouseEvent& e);
	};
	wxwin_mainwindow();
	void request_paint();
	void notify_update() throw();
	void notify_exit() throw();
	void on_close(wxCloseEvent& e);
	void menu_start(wxString name);
	void menu_special(wxString name, wxMenu* menu);
	void menu_special_sub(wxString name, wxMenu* menu);
	void menu_entry(int id, wxString name);
	void menu_entry_check(int id, wxString name);
	void menu_start_sub(wxString name);
	void menu_end_sub(wxString name);
	bool menu_ischecked(int id);
	void menu_check(int id, bool newstate);
	void menu_separator();
	void handle_menu_click(wxCommandEvent& e);
	void menu_edit_keybindings(wxCommandEvent& e);
	void menu_edit_aliases(wxCommandEvent& e);
	void menu_edit_memorywatch(wxCommandEvent& e);
	void menu_save_memorywatch(wxCommandEvent& e);
	void menu_load_memorywatch(wxCommandEvent& e);
private:
	panel* gpanel;
	wxMenu* current_menu;
	wxMenuBar* menubar;
	std::map<int, wxMenuItem*> checkitems;
	std::stack<wxMenu*> upper;
	void* ahmenu;
	void* sounddev;
	void* dmenu;
};

#endif
