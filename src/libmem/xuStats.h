#include <stdio.h>
#include <stdlib.h>
#include "Resource.h"
#include "GProcessor.h"

#ifndef  XUSTATS_H
#define  XUSTATS_H

#define xu_nCPU     9
#define xu_nThread  xu_nCPU
#define current_ID  0
#define max_ID      1
#define temp_NUM    2
#define control_NUM 3
#define xu_nPhase  32
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


struct statDataContext{
	long long sumInst;
	long long clock;
	double    IPC;
	double    power;
	double    energy;
	double    pw;
};
class xuStats{
	
private:
	FILE   *totFp;
	char   *totFileName;
	FILE   *fp;
	bool   pre[xu_nThread];
	char   *fileName;
	struct statsData *preData;
	struct statsData *thisData;
	struct statDataContext  context[xu_nCPU];
	long long  phase_Z[xu_nThread][4];
	long long  control_table[xu_nCPU][control_NUM + 1];
	long long  phase_table[xu_nCPU][xu_nPhase][4];
	double     phase_pw[xu_nThread][xu_nPhase][xu_nCPU];
	long long  interval;
	long long  totalNInst;
	long long  statInst[xu_nThread];
	double     ITV_diff;
	int  sigma_curr[xu_nThread];  //thread -> cpu
	int  sigma_dst[xu_nThread];  //thread -> cpu
        int  arc_sigma[xu_nCPU]; //cpu->thread
        
	bool is_Done[xu_nThread];
	bool is_Free[xu_nCPU];
        int nBegin;

	bool is_Migrate(int *curr, int *dst);
	void doMigrate(int *curr, int *dst);
	void swap(int * a, int *b){
		int temp;
		temp = *a;
		*a = *b;
		*b = temp;
	}
	void dfs(double **matrix, int *dst,int *A,double &max,int start, int end);
        void outputDataThread(int cpuId);
	void saveDataContext(int cpuId);
public:
	xuStats(char *fileName,char *totFileName,int nCPUs);
	void getStatData(GProcessor *core);
	void inputToFile(GProcessor *core);
	void getTotStats(GProcessor *core);
};

#endif
