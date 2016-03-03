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
    pre = false;
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
    for(int i = 0; i < xu_nCPU; i++){
    	for( int j = 0; j < control_NUM; j++){
	    control_table[i][j] = -1;
	}

	control_table[i][temp_NUM] = 0;
    }
     memset(&phase_table[0][0][0],0,9 * 16 * 4 * sizeof(long long));
     memset(&phase_Z[0],0,4 * sizeof(long long));
     interval = 100000;
     ITV_diff = 0.050;
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
    long long INT = diffiALU + diffiMult + diffiDiv;
    long long BJ  = diffiBJ;
    long long FP  = difffpALU + difffpMult + difffpDiv;
    long long MEM = diffiStore + diffiLoad;
    long long diffINT;
    long long diffBJ;
    long long diffFP;
    long long diffMEM;
    double delt;
    int tempid    = control_table[cpuId][max_ID] + 1;
    int currentid = control_table[cpuId][current_ID];
    if(0 == tempid){ // for the first phase
        if(pre == false ){
		phase_Z[0] = INT;
		phase_Z[1] = BJ;
		phase_Z[2] = FP;
		phase_Z[3] = MEM;
		pre = true;
	}
	else{
		diffINT = abs(phase_Z[0] - INT); 
		diffBJ  = abs(phase_Z[1] - BJ); 
		diffFP  = abs(phase_Z[2] - FP);
		diffMEM = abs(phase_Z[3] - MEM);
		delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;
		if(delt > ITV_diff){
			control_table[cpuId][temp_NUM] = 0;
			phase_Z[0] = INT;
			phase_Z[1] = BJ;
			phase_Z[2] = FP;
			phase_Z[3] = MEM;
		}
		else {
			if (++control_table[cpuId][temp_NUM] >= 4){	  
				control_table[cpuId][temp_NUM] = 0;
				control_table[cpuId][max_ID] = tempid;
				control_table[cpuId][current_ID] = tempid;
				phase_table[cpuId][tempid][0] = phase_Z[0];
				phase_table[cpuId][tempid][1] = phase_Z[1];
				phase_table[cpuId][tempid][2] = phase_Z[2];
				phase_table[cpuId][tempid][3] = phase_Z[3];
			}
		}
	}
    }
    else{ //other phase
		diffINT = abs(phase_table[cpuId][currentid][0] - INT); 
		diffBJ  = abs(phase_table[cpuId][currentid][1] - BJ); 
		diffFP  = abs(phase_table[cpuId][currentid][2] - FP); 
		diffMEM = abs(phase_table[cpuId][currentid][3] - MEM); 
		delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;
//	printf("%lf\n", delt);
	if( delt >  ITV_diff){ //may be a new phase  // may a previous phase
		if(++control_table[cpuId][temp_NUM] >= 4){// is another phase
			control_table[cpuId][temp_NUM] = 0;
			for(int i = 0; i <= control_table[cpuId][max_ID]; i++){ //Is previous phase?
				diffINT = abs(phase_table[cpuId][i][0] - INT); 
				diffBJ  = abs(phase_table[cpuId][i][1] - BJ); 
				diffFP  = abs(phase_table[cpuId][i][2] - FP); 
				diffMEM = abs(phase_table[cpuId][i][3] - MEM); 
				delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;

				if(delt <= ITV_diff){
					control_table[cpuId][max_ID]--;
					tempid = i;
			//		printf("zhi shaoyiciba\n");
					break;
				}
			}
			control_table[cpuId][max_ID]++;
			control_table[cpuId][current_ID] = tempid;
			phase_table[cpuId][tempid][0] = INT;
			phase_table[cpuId][tempid][1] = BJ;
			phase_table[cpuId][tempid][2] = FP;
			phase_table[cpuId][tempid][3] = MEM;

		}
	}
	else{ //is current
		control_table[cpuId][temp_NUM] = 0;
	}
    }
     
    //long long diffiFence= thisData[cpuId].nInst[iFence]; 
    //long long diffiEvent= thisData[cpuId].nInst[iEvent]; 


    fprintf(fp,"    %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",diffiALU,diffiMult,diffiDiv,diffiBJ,diffiLoad,diffiStore,difffpALU,difffpMult,difffpDiv,control_table[cpuId][current_ID]);
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














