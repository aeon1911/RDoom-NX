// Prints what the RDoom launcher sees in game files (diagnostics / docs).
//   rdoom_inspect <iwadinfo.txt|game_support.pk3> <file>...
#include <cstdio>
#include "content.h"
#include "log.h"
using namespace rdoom;
int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: %s <iwadinfo> <file>...\n", argv[0]); return 2; }
	LogSetLevel(LogLevel::Warn);
	auto defs = LoadIwadDefs(argv[1]);
	for (int i = 2; i < argc; i++)
	{
		ContentInfo c = InspectFile(argv[i], defs);
		printf("%s\n  status=%s%s%s size=%llu iwad=%d def=%s title=\"%s\"\n  category=%s maps=%d zscript=%d decorate=%d dehacked=%d mapinfo=%d titlepic=%d\n",
			c.fileName.c_str(), FileStatusName(c.status), c.error.empty() ? "" : " error=", c.error.c_str(),
			(unsigned long long)c.size, c.isIwad, c.iwadDef >= 0 ? defs[c.iwadDef].name.c_str() : "-", c.title.c_str(),
			ModCategoryName(c.category), c.mapCount, c.hasZScript, c.hasDecorate, c.hasDehacked, c.hasMapinfo, c.hasTitlePic);
		if (!c.gameinfoIwad.empty() || !c.gameinfoLoad.empty())
		{
			printf("  gameinfo iwad=%s load=", c.gameinfoIwad.c_str());
			for (auto &l : c.gameinfoLoad) printf("%s ", l.c_str());
			printf("\n");
		}
	}
	return 0;
}
