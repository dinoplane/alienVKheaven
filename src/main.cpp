#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>


#include <vk_engine.h>


#undef main
int main(int argc, char* argv[])
{
	 VulkanEngine engine;

	 engine.init();	
	 engine.run();	
	 engine.cleanup();	

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG); 
    _CrtDumpMemoryLeaks();
	return 0;
}
