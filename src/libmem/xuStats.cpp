#include "Cache.h"
#include "GProcessor.h"
#include "GMemorySystem.h"
#include "MemObj.h"
#include "MemorySystem.h"
#include "Processor.h"
#include "xuStats.h"
#include "OSSim.h"
xuStats::xuStats(char *fileName,char *totFileName,int nCPUs){
    this->fileName = fileName;
    this->totFileName = totFileName;
   /* preData.dl1 = 0;
    preData.il1 = 0;
    preData.l2 = 0;
    preData.bpred = 0;
    preData.energy = 0;
    preData.IPC = 0;*/
    fp = NULL;
    totFp = NULL;
    preData  = new statsData[nCPUs];
    thisData = new statsData[nCPUs];
    for(int i = 0; i < nCPUs; i++){
       preData[i].dl1 = 0;
       preData[i].il1 = 0;
       preData[i].l2 = 0;
       preData[i].bpred = 0;
       preData[i].energy = 0.0;
       preData[i].clock = 0;
       memset(preData[i].nInst,0,MaxInstType * sizeof(long long));
       preData[i].sumInst = 0;
       preData[i].IPC = 0.0;
       preData[i].energyEffiency = 0.0;
    }

}

void xuStats::getStatData(GProcessor *proc){
    int cpuId = proc->getId();
    GMemorySystem *gm  = proc->getMemorySystem();
    MemObj *dataSource = gm->getDataSource();  //dl1
    MemObj *instSource = gm->getInstrSource(); //il1
    MemObj *l2Cache    = (*(instSource->getLowerLevel())).front();
    
    GStatsCntr **ppnInst = proc->xuGetNInst(); 
    thisData[cpuId].sumInst = 0;
    for(int i = iOpInvalid; i < MaxInstType; i++){ //the number of instruction per  type 
	int temp                 = ppnInst[i]->getValue(); 
	thisData[cpuId].nInst[i] = temp;
    	thisData[cpuId].sumInst += temp; ///the number of instruction for IPC
    }
    //get dl1
    dataSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].dl1 = thisData[cpuId].missValue;
    //get il1
    instSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].il1 = thisData[cpuId].missValue;
    //get clock for IPC
    thisData[cpuId].clock = proc->getClockTicks();   //clock


    //get l2 cache miss
    l2Cache->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].l2 = thisData[cpuId].missValue;
    
    //get energy
    const char * procName = SescConf->getCharPtr("","cpucore",cpuId); 
    double pPower         = EnergyMgr::etop(GStatsEnergy::getTotalProc(cpuId));
    double maxClockEnergy = EnergyMgr::get(procName,"clockEnergy",cpuId);
    double maxEnergy      = EnergyMgr::get(procName,"totEnergy");
    // More reasonable clock energy. 50% is based on activity, 50% of the clock
    // distributtion is there all the time
    double clockPower     = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
    double corePower      = pPower + clockPower;
    double coreEnergy     = EnergyMgr::ptoe(corePower);
    thisData[cpuId].energy= coreEnergy;
}


void xuStats::inputToFile(GProcessor *proc){
    int cpuId = proc->getId();
    fp = fopen(fileName,"a+");
    if(NULL == fp){
    	printf("Can't open file : %s\n",fileName);
	exit(-1);
    }
    long long diffdl1    = thisData[cpuId].dl1 - preData[cpuId].dl1;
    long long diffil1    = thisData[cpuId].il1 - preData[cpuId].il1;
    long long diffl2     = thisData[cpuId].l2  - preData[cpuId].l2;
    
    long long diffclock  = thisData[cpuId].clock - preData[cpuId].clock;
    long long diffinst   = thisData[cpuId].sumInst - preData[cpuId].sumInst;
    double    diffIPC    = (double)diffinst / (double)diffclock;
    
    double    diffenergy = thisData[cpuId].energy - preData[cpuId].energy;
    double    diffpower  = diffenergy / diffclock*(osSim->getFrequency()/1e9);
    //print to result file
    fprintf(fp,"%d %lld %lld %lld %lf %lf %lf %lf ",cpuId,diffdl1,diffil1,diffl2,diffIPC,diffenergy,diffpower,diffIPC/diffpower);
    
    long long diffiALU  = thisData[cpuId].nInst[iALU] -  preData[cpuId].nInst[iALU]; 
    long long diffiMult = thisData[cpuId].nInst[iMult] - preData[cpuId].nInst[iMult];  
    long long diffiDiv  = thisData[cpuId].nInst[iDiv] -  preData[cpuId].nInst[iDiv]; 
    long long diffiBJ   = thisData[cpuId].nInst[iBJ] -   preData[cpuId].nInst[iBJ]; 
    long long diffiLoad = thisData[cpuId].nInst[iLoad] - preData[cpuId].nInst[iLoad]; 
    long long diffiStore= thisData[cpuId].nInst[iStore] -preData[cpuId].nInst[iStore]; 
    long long difffpALU = thisData[cpuId].nInst[fpALU] - preData[cpuId].nInst[fpALU]; 
    long long difffpMult= thisData[cpuId].nInst[fpMult]- preData[cpuId].nInst[fpMult]; 
    long long difffpDiv = thisData[cpuId].nInst[fpDiv] - preData[cpuId].nInst[fpDiv]; 
    //long long diffiFence= thisData[cpuId].nInst[iFence]; 
    //long long diffiEvent= thisData[cpuId].nInst[iEvent]; 


    fprintf(fp,"    %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",diffiALU,diffiMult,diffiDiv,diffiBJ,diffiLoad,diffiStore,difffpALU,difffpMult,difffpDiv);
    //save to the previous result
    preData[cpuId].dl1   =  thisData[cpuId].dl1;
    preData[cpuId].il1   =  thisData[cpuId].il1;
    preData[cpuId].l2    =  thisData[cpuId].l2;
    preData[cpuId].clock =  thisData[cpuId].clock;
    preData[cpuId].sumInst =  thisData[cpuId].sumInst;
    preData[cpuId].energy =  thisData[cpuId].energy;
    
    for(int i = iOpInvalid; i < MaxInstType; i++){
	preData[cpuId].nInst[i] = thisData[cpuId].nInst[i];
    } // no aim
    fclose(fp);
}
void xuStats::getTotStats(GProcessor *proc){
	//get app's whole statistical data 
	//such as IPC, Power ,IPC/Power etc.
      int cpuId       = 0;
      long long clock = 0;
      double IPC      = 0.0;
      long long sumInst = 0;
      if(proc->getClockTicks() > 0){
	   cpuId = proc->getId();
   	   clock = proc->getClockTicks();   // clock 

           GStatsCntr **ppnInst = proc->xuGetNInst();  //sumInst
           for(int i = iOpInvalid; i < MaxInstType; i++){
	   	sumInst += ppnInst[i]->getValue(); 
	   }
	   IPC = (double)sumInst / (double)clock;  //IPC
           const char * procName = SescConf->getCharPtr("","cpucore",cpuId); 
           double pPower         = EnergyMgr::etop(GStatsEnergy::getTotalProc(cpuId));
           double maxClockEnergy = EnergyMgr::get(procName,"clockEnergy",cpuId);
           double maxEnergy      = EnergyMgr::get(procName,"totEnergy");
    // More reasonable clock energy. 50% is based on activity, 50% of the clock
    // distributtion is there all the time
           double clockPower     = 0.5 * (maxClockEnergy/maxEnergy) * pPower + 0.5 * maxClockEnergy;
           double corePower      = pPower + clockPower;      //energy
           double coreEnergy     = EnergyMgr::ptoe(corePower);   //power
   	     
           totFp = fopen(totFileName,"a+");
           if(NULL == totFp){
    	         printf("Can't open file : %s\n",fileName);
	          exit(-1);
            }
	   fprintf(totFp,"%d %lf %lf %lf %lf\n",cpuId,IPC,corePower,coreEnergy,IPC/corePower);
      	   fclose(totFp);
       }
}














