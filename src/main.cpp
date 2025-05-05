// #define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
// #include <crtdbg.h>


#include <vk_engine.h>


#undef main
int main()
{
	 VulkanEngine engine;

	  engine.Init();	
	  engine.Run();	
	  engine.Cleanup();	

    // _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG); 
    // _CrtDumpMemoryLeaks();
	return 0;
}
