#include <stdio.h>
#include <stdlib.h>
#include "Resource.h"
#include "GProcessor.h"

#ifndef  XUSTATS_H
#define  XUSTATS_H

#define xu_nCPU     9
#define current_ID  0
//#define temp_ID     1
#define max_ID      1
#define temp_NUM    2
#define control_NUM 3
struct statsData{
	long long dl1miss;
	long long il1miss;
	long long l2miss;
	long long dl1hit;
	long long il1hit;
	long long l2hit;
	long long itlb;
	long long dtlb;
	long long missValue;
	long long hitValue;
	long long nbranche;
	long long nbranchMiss;
	long long nbranchHit;
	double    energy;
	long long clock;
	long long nInst[MaxInstType];
	long long nStall[MaxStall];
	long long sumInst;
	double    IPC;
	double    power;
	double    energyEffiency; 
};
class xuStats{
	
private:
	FILE *totFp;
	char *totFileName;
	FILE *fp;
	bool pre;
	char *fileName;
	struct statsData *preData;
	struct statsData *thisData;
	long long  phase_Z[4];
	long long control_table[xu_nCPU][max_ID + 1];
	long long  phase_table[xu_nCPU][16][4];
	long long  interval;
	double     ITV_diff;
public:
	xuStats(char *fileName,char *totFileName,int nCPUs);
	void getStatData(GProcessor *proc);
	void inputToFile(GProcessor *proc);
	void getTotStats(GProcessor *proc);
};

#endif
