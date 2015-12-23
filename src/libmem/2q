
#include "Cache.h"
#include "GProcessor.h"
#include "GMemorySystem.h"
#include "MemObj.h"
#include "MemorySystem.h"
#include "Processor.h"
#include "xuStats.h"

xuStats::xuStats(char *fileName,int nCPUs){
    this->fileName = fileName;
   /* preData.dl1 = 0;
    preData.il1 = 0;
    preData.l2 = 0;
    preData.bpred = 0;
    preData.energy = 0;
    preData.IPC = 0;*/
    this->fp = NULL;
    preData  = new statsData[nCPUs];
    thisData = new statsData[nCPUs];
    for(int i = 0; i < nCPUs; i++){
       preData[i].dl1 = 0;
       preData[i].il1 = 0;
       preData[i].l2 = 0;
       preData[i].bpred = 0;
       preData[i].energy = 0.0;
       preData[i].clock = 0;
       preData[i].nInst = 0;
       preData[i].IPC = 0.0;
    }

}

void xuStats::getStatData(GProcessor *proc){
    int cpuId = proc->getId();
    GMemorySystem *gm = proc->getMemorySystem();
    MemObj *dataSource = gm->getDataSource();  //dl1
    MemObj *instSource = gm->getInstrSource(); //il1
    MemObj *l2Cache   = (*(instSource->getLowerLevel())).front();
   // if(l2Chache != NULL)
    //printf("isCache : %s\n",l2Chache->isCache()?"True":"false");
    //get dl1
    dataSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].dl1 = thisData[cpuId].missValue;
    //get il1
    instSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].il1 = thisData[cpuId].missValue;
    //get clock, nInst, IPC
    thisData[cpuId].clock = proc->getClockTicks();   //clock
    thisData[cpuId].nInst = 0;
    //get l2 cache miss
    l2Cache->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].l2 = thisData[cpuId].missValue;
}


void xuStats::inputToFile(GProcessor *proc){
    int cpuId = proc->getId();
    fp = fopen(fileName,"a+");
    if(NULL == fp){
    	printf("Can't open file : %s\n",fileName);
	exit(-1);
    }
    long long diffdl1 = thisData[cpuId].dl1 - preData[cpuId].dl1;
    long long diffil1 = thisData[cpuId].il1 - preData[cpuId].il1;
    long long diffl2  = thisData[cpuId].l2  - preData[cpuId].l2;
    fprintf(fp,"%d %lld %lld %lld\n",cpuId,diffdl1,diffil1,diffl2);
    preData[cpuId].dl1 =  thisData[cpuId].dl1;
    preData[cpuId].il1 =  thisData[cpuId].il1;
    preData[cpuId].l2  =  thisData[cpuId].l2;
    fclose(fp);
}
