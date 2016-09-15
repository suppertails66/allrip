CXX = g++
CFLAGS = -Wall
CFILES = *.cpp modules/*.cpp utils/*.cpp
CDEFINES = 
MAKEATLAS = -DENABLE_ATLAS
MAKECANDYADV = -DENABLE_CANDYADV
MAKEHUMONGOUS = -DENABLE_HUMONGOUS
MAKEINDIAN = -DENABLE_INDIAN
MAKELEGOISLAND = -DENABLE_LEGOISLAND
MAKEMOHAWK = -DENABLE_MOHAWK
MAKEALL = -DENABLE_ATLAS -DENABLE_CANDYADV -DENABLE_HUMONGOUS -DENABLE_INDIAN -DENABLE_LEGOISLAND -DENABLE_MOHAWK

all: atlasrip candyadvrip humongousrip indianrip legoislandrip mohawkrip

allrip:
	g++ $(CFLAGS) $(MAKEALL) $(CFILES) -o allrip

atlasrip:
	g++ $(CFLAGS) $(MAKEATLAS) $(CFILES) -o atlasrip

candyadvrip:
	g++ $(CFLAGS) $(MAKECANDYADV) $(CFILES) -o candyadvrip

humongousrip:
	g++ $(CFLAGS) $(MAKEHUMONGOUS) $(CFILES) -o humongousrip

indianrip:
	g++ $(CFLAGS) $(MAKEINDIAN) $(CFILES) -o indianrip

legoislandrip:
	g++ $(CFLAGS) $(MAKELEGOISLAND) $(CFILES) -o legoislandrip
	
mohawkrip:
	g++ $(CFLAGS) $(MAKEMOHAWK) $(CFILES) -o mohawkrip

.PHONY: clean

clean:
	rm -f allrip
	rm -f atlasrip
	rm -f candyadvrip
	rm -f humongousrip
	rm -f indianrip
	rm -f legoislandrip
	rm -f mohawkrip

