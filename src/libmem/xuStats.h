#include <stdio.h>
#include <stdlib.h>

#include "GProcessor.h"

#ifndef  XUSTATS_H
#define  XUSTATS_H
struct statsData{
	long long  dl1;
	long long il1;
	long long l2;
	long long bpred;
	long long missValue;
	double energy;
	long long clock;
	long long nInst[MaxInstType];
	long long sumInst;
	double IPC;
	double power;
	double energyEffiency; 
};
class xuStats{
	
private:
	FILE *fp;
	char *fileName;
	struct statsData *preData;
	struct statsData *thisData;
public:
	xuStats(char *fileName,int nCPUs);
	void getStatData(GProcessor *proc);
	void inputToFile(GProcessor *proc);
	void getTotStats(GProcessor *proc);
};

#endif
