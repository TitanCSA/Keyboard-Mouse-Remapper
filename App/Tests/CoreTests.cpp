#include "Test.h"
#include "../Core/Script.h"
#include "../Core/VisualDocument.h"
#include <filesystem>
#include <fstream>
using namespace gpr;
int main(int argc,char** argv){try{
 Check(Controls::Parse("KEY_A")==30&&Controls::Parse("E0_1D")==285,"Physical scan codes and extended controls differ");
 Check(Controls::Parse("KEY_RCTRL")==285&&Controls::Parse("MOUSE_X")==MouseX,"Named keyboard/mouse controls resolve");
 Check(Controls::Parse("LX")==-1,"Controller names must not be silently mapped");
 for(int i=0;i<ControlCount;++i)if(!Controls::Names()[i].empty())Check(Controls::Parse(Controls::Names()[i])==i,"Control round trip");
 auto p=ScriptCompiler::Compile("ui { slider speed(\"Speed\",0,1000,50,\"%\"); } main { sensitivity(MOUSE_X,speed); }");
 auto vm=p->CreateInstance();ControlFrame f;f.Available.fill(true);int sum=0;
 for(int i=0;i<10000;++i){f.Physical[MouseX]=1;vm->Execute(f,i*1000,1);sum+=f.Values[MouseX];}
 Check(sum==5000,"Fractional mouse sensitivity must retain sub-count motion");
 for(int i=0;i<10000;++i){f.Physical[MouseX]=-1;vm->Execute(f,(10000+i)*1000,1);sum+=f.Values[MouseX];}
 Check(sum==0,"Fractional scaling must not drift for opposite movements");
 vm=ScriptCompiler::Compile("main { sensitivity(MOUSE_X,200); }")->CreateInstance();f.Physical[MouseX]=12000;vm->Execute(f,0,0);Check(f.Values[MouseX]==24000,"Mouse deltas must not be clamped to gamepad percentages");
 vm=ScriptCompiler::Compile("main { if(get_ival(KEY_LCTRL) && get_ival(MOUSE_RIGHT)) set_val(KEY_C,100); }")->CreateInstance();
 f.Physical.fill(0);f.Physical[29]=f.Physical[MouseRight]=100;vm->Execute(f,0,0);Check(f.Values[46]==100,"Cross-family chords work in one frame");
 auto doc=VisualDocument::Parse("main { // preserved code\n }");doc.Rules.push_back({VisualRuleKind::Remap,"KEY_A","KEY_B",0,true});const auto built=doc.BuildSource();ScriptCompiler::Compile(built);const auto parsed=VisualDocument::Parse(built);Check(parsed.Rules.size()==1,"Visual/code round trip");
 bool fault=false;try{auto bad=ScriptCompiler::Compile("main { while(1) {} }")->CreateInstance();bad->Execute(f,0,0);}catch(const ScriptException&){fault=true;}Check(fault,"Unbounded scripts must stop at the instruction budget");
 if(argc>1)for(const auto& path:std::filesystem::directory_iterator(argv[1]))if(path.path().extension()==".igc"){std::ifstream file(path.path());const std::string text((std::istreambuf_iterator<char>(file)),{});try{ScriptCompiler::Compile(text);}catch(const ScriptException& e){throw std::runtime_error(path.path().filename().string()+": "+e.Diagnostic());}++checks;}
 std::cout<<checks<<" language/visual checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
