#include "Script.h"
#include <unordered_map>
#include <cstdio>
namespace gpr {
namespace {
std::string Upper(std::string s){for(auto& c:s)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');return s;}
struct NamedKey {const char* name;int scan;};
constexpr NamedKey keys[]={
{"ESC",1},{"1",2},{"2",3},{"3",4},{"4",5},{"5",6},{"6",7},{"7",8},{"8",9},{"9",10},{"0",11},
{"MINUS",12},{"EQUAL",13},{"BACKSPACE",14},{"TAB",15},{"Q",16},{"W",17},{"E",18},{"R",19},{"T",20},{"Y",21},{"U",22},{"I",23},{"O",24},{"P",25},
{"LBRACKET",26},{"RBRACKET",27},{"ENTER",28},{"LCTRL",29},{"A",30},{"S",31},{"D",32},{"F",33},{"G",34},{"H",35},{"J",36},{"K",37},{"L",38},
{"SEMICOLON",39},{"APOSTROPHE",40},{"GRAVE",41},{"LSHIFT",42},{"BACKSLASH",43},{"Z",44},{"X",45},{"C",46},{"V",47},{"B",48},{"N",49},{"M",50},
{"COMMA",51},{"PERIOD",52},{"SLASH",53},{"RSHIFT",54},{"NUM_MULTIPLY",55},{"LALT",56},{"SPACE",57},{"CAPSLOCK",58},
{"F1",59},{"F2",60},{"F3",61},{"F4",62},{"F5",63},{"F6",64},{"F7",65},{"F8",66},{"F9",67},{"F10",68},{"NUMLOCK",69},{"SCROLLLOCK",70},
{"NUM_7",71},{"NUM_8",72},{"NUM_9",73},{"NUM_SUBTRACT",74},{"NUM_4",75},{"NUM_5",76},{"NUM_6",77},{"NUM_ADD",78},{"NUM_1",79},{"NUM_2",80},{"NUM_3",81},{"NUM_0",82},{"NUM_DECIMAL",83},
{"F11",87},{"F12",88},{"F13",100},{"F14",101},{"F15",102},{"F16",103},{"F17",104},{"F18",105},{"F19",106},{"F20",107},{"F21",108},{"F22",109},{"F23",110},{"F24",118},
{"RCTRL",0x11d},{"RALT",0x138},{"NUM_ENTER",0x11c},{"NUM_DIVIDE",0x135},{"HOME",0x147},{"UP",0x148},{"PAGEUP",0x149},{"LEFT",0x14b},{"RIGHT",0x14d},
{"END",0x14f},{"DOWN",0x150},{"PAGEDOWN",0x151},{"INSERT",0x152},{"DELETE",0x153},{"LWIN",0x15b},{"RWIN",0x15c},{"MENU",0x15d},{"PRINTSCREEN",0x137},{"PAUSE",0x245},
{"VOLUME_UP",0x130},{"VOLUME_DOWN",0x12e},{"MUTE",0x120},{"MEDIA_PLAY",0x122},{"MEDIA_NEXT",0x119},{"MEDIA_PREVIOUS",0x110},{"MEDIA_STOP",0x124}};
}
const std::array<std::string,ControlCount>& Controls::Names(){
 static const auto names=[]{
  std::array<std::string,ControlCount> n{};
  for(int bank=0;bank<3;++bank)for(int scan=0;scan<256;++scan){char label[24]{};sprintf_s(label,bank==0?"SCAN_%02X":bank==1?"E0_%02X":"E1_%02X",scan);n[bank*256+scan]=label;}
  for(const auto& k:keys)n[k.scan]="KEY_"+std::string(k.name);
  const char* mouse[]={"MOUSE_LEFT","MOUSE_RIGHT","MOUSE_MIDDLE","MOUSE_X1","MOUSE_X2","MOUSE_X","MOUSE_Y","MOUSE_WHEEL","MOUSE_HWHEEL","ABS_X","ABS_Y"};
  for(int i=0;i<11;++i)n[MouseLeft+i]=mouse[i];
  return n;
 }();return names;
}
int Controls::Parse(const std::string& name){
 static const auto lookup=[]{
  std::unordered_map<std::string,int> m;
  for(int i=0;i<ControlCount;++i)if(!Names()[i].empty())m[Names()[i]]=i;
  for(int bank=0;bank<3;++bank)for(int scan=0;scan<256;++scan){char label[24]{};sprintf_s(label,bank==0?"SCAN_%02X":bank==1?"E0_%02X":"E1_%02X",scan);m[label]=bank*256+scan;}
  m["CTRL"]=29;m["SHIFT"]=42;m["ALT"]=56;m["SPACE"]=57;m["KEY_CTRL"]=29;m["KEY_SHIFT"]=42;m["KEY_ALT"]=56;
  m["MOUSE_BUTTON_4"]=MouseX1;m["MOUSE_BUTTON_5"]=MouseX2;
  return m;
 }();const auto found=lookup.find(Upper(name));return found==lookup.end()?-1:found->second;
}
ScriptException::ScriptException(const std::string& message,int line,int column):std::runtime_error(message),Line(line),Column(column){}
std::string ScriptException::Diagnostic()const{return "Line "+std::to_string(Line)+", column "+std::to_string(Column)+": "+what();}
}