#include "Cache.h"
#include "GProcessor.h"
#include "GMemorySystem.h"
#include "MemObj.h"
#include "MemorySystem.h"
#include "Processor.h"
#include "xuStats.h"
#include "OSSim.h"
#include "ProcessId.h"
xuStats::xuStats(char *fileName,char *totFileName,int nCPUs){
    this->fileName = fileName;
    this->totFileName = totFileName;
    pre = false;
    fp = NULL;
    totFp = NULL;
    preData  = new statsData[nCPUs];
    thisData = new statsData[nCPUs];
    for(int i = 0; i < nCPUs; i++){
       preData[i].dl1miss    = 0;
       preData[i].il1miss    = 0;
       preData[i].l2miss     = 0;
       preData[i].dl1hit     = 0;
       preData[i].il1hit     = 0;
       preData[i].l2hit      = 0;
       preData[i].itlb   = 0;
       preData[i].dtlb   = 0;
       preData[i].nbranche = 0;
       preData[i].nbranchMiss = 0;
       preData[i].nbranchHit = 0;
       preData[i].energy = 0.0;
       preData[i].clock  = 0;
       memset(preData[i].nInst,0,MaxInstType * sizeof(long long));
       memset(preData[i].nStall,0,MaxStall * sizeof(long long));
       preData[i].sumInst = 0;
       preData[i].IPC = 0.0;
       preData[i].energyEffiency = 0.0;
       
       context[i].sumInst  = 0;
       context[i].clock    = 0;
       context[i].IPC      = 0;
       context[i].power    = 0;
       context[i].energy   = 0;
       context[i].pw       = 0;
	
    }
    for(int i = 0; i < xu_nCPU; i++){
    	for( int j = 0; j < control_NUM; j++){
	    control_table[i][j] = -1;
	}

	control_table[i][temp_NUM] = 0;
    }
    for(int i = 0; i < xu_nThread; i++){
	    statInst[i] = 0;
    }


    for(int i = 0; i < xu_nThread; i++){
	    sigma_curr[i] = i;
    }

    for(int i = 0; i < xu_nThread; i++){
	    sigma_dst[i] = i;
    }

    for(int i = 0; i < xu_nCPU; i++){
	    arc_sigma[i] = i;
    }
     memset(&phase_table[0][0][0],0,xu_nThread * xu_nPhase * 4 * sizeof(long long));
     memset(&phase_pw[0][0][0],0,xu_nThread * xu_nPhase * xu_nCPU * sizeof(double));
     memset(&phase_Z[0],0,4 * sizeof(long long));
     memset(&statInst[0],0,xu_nPhase * sizeof(long long));

     interval = 100000;
     totalNInst = 100000000;
     ITV_diff = 0.050;
}

void xuStats::getStatData(GProcessor *proc){
    int cpuId = proc->getId();
    int threadId = arc_sigma[cpuId];
    GMemorySystem *gm  = proc->getMemorySystem();
    MemObj *dataSource = gm->getDataSource();  //dl1
    MemObj *instSource = gm->getInstrSource(); //il1
    MemObj *l2Cache    = (*(instSource->getLowerLevel())).front();
    BPredictor *bpred  = proc->xuGetBPred();

   // get instructions    
    GStatsCntr **ppnInst = proc->xuGetNInst(); 
    thisData[cpuId].sumInst = 0;
    for(int i = iOpInvalid; i < MaxInstType; i++){ //the number of instruction per  type 
	int temp                 = ppnInst[i]->getValue(); 
	thisData[cpuId].nInst[i] = temp;
    	thisData[cpuId].sumInst += temp; ///the number of instruction for IPC
    }
    //get stall
    GStatsCntr **ppnStall = proc->xuGetNStall();
    for(int i = NoStall + 1; i < MaxStall; i++){
    	thisData[cpuId].nStall[i] = ppnStall[i]->getValue();
    }
    //get dl1
    dataSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].dl1miss = thisData[cpuId].missValue;
    thisData[cpuId].dl1hit  = thisData[cpuId].hitValue;
    
    //get il1
    instSource->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].il1miss = thisData[cpuId].missValue;
    thisData[cpuId].il1hit  = thisData[cpuId].hitValue;
    //get clock for IPC
    thisData[cpuId].clock = proc->getClockTicks();   //clock
  
    //get l2 cache miss
    l2Cache->xuGetStatsData(&thisData[cpuId]);
    thisData[cpuId].l2miss = thisData[cpuId].missValue;
    thisData[cpuId].l2hit  = thisData[cpuId].hitValue;
    
    //get itlb
    thisData[cpuId].itlb = gm->getMemoryOS()->xuGetITLBMiss()->getValue();    
    //get dtlb  
    thisData[cpuId].dtlb = gm->getMemoryOS()->xuGetDTLBMiss()->getValue();    
    //bpred
    thisData[cpuId].nbranche    = bpred->xuGetNBranche();
    thisData[cpuId].nbranchMiss = bpred->xuGetNBrancheMiss();
    thisData[cpuId].nbranchHit  = thisData[cpuId].nbranche - thisData[cpuId].nbranchMiss;


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
    int threadId = arc_sigma[cpuId];
    fp = fopen(fileName,"a+");
    if(NULL == fp){
    	printf("Can't open file : %s\n",fileName);
	exit(-1);
    }
    //performance event
    long long diffdl1miss    = thisData[cpuId].dl1miss - preData[cpuId].dl1miss;
    long long diffil1miss    = thisData[cpuId].il1miss - preData[cpuId].il1miss;
    long long diffl2miss     = thisData[cpuId].l2miss  - preData[cpuId].l2miss;

    long long diffdl1hit     = thisData[cpuId].dl1hit - preData[cpuId].dl1hit;
    long long diffil1hit     = thisData[cpuId].il1hit - preData[cpuId].il1hit;
    long long diffl2hit      = thisData[cpuId].l2hit  - preData[cpuId].l2hit;
    long long diffitlb       = thisData[cpuId].itlb  - preData[cpuId].itlb;
    long long diffdtlb       = thisData[cpuId].dtlb  - preData[cpuId].dtlb;
    long long diffclock      = thisData[cpuId].clock - preData[cpuId].clock;
    long long diffinst       = thisData[cpuId].sumInst - preData[cpuId].sumInst;
    double    diffIPC        = (double)diffinst / (double)diffclock;
    double    diffenergy     = thisData[cpuId].energy - preData[cpuId].energy;
    double    diffpower      = diffenergy / diffclock*(osSim->getFrequency()/1e9);
    double    L1MissRate     = (double)(diffdl1miss + diffil1miss)/(double)(diffdl1miss + diffil1miss + diffdl1hit + diffil1hit);
    double    L2MissRate     = (double)diffl2miss/(double)(diffl2miss + diffl2hit);
    long long diffNBranch    = thisData[cpuId].nbranche - preData[cpuId].nbranche;
    long long diffNBranchMiss= thisData[cpuId].nbranchMiss - preData[cpuId].nbranchMiss;
    long long diffNBranchHit = thisData[cpuId].nbranchHit - preData[cpuId].nbranchHit;
  ///Stall 
    long long diffWinStall          = thisData[cpuId].nStall[SmallWinStall]     - preData[cpuId].nStall[SmallWinStall]; 
    long long diffROBStall          = thisData[cpuId].nStall[SmallROBStall]     - preData[cpuId].nStall[SmallROBStall];  
    long long diffREGStall          = thisData[cpuId].nStall[SmallREGStall]     - preData[cpuId].nStall[SmallREGStall]; 
    long long diffLoadStall         = thisData[cpuId].nStall[OutsLoadsStall]    - preData[cpuId].nStall[OutsLoadsStall]; 
    long long diffStoreStall        = thisData[cpuId].nStall[OutsStoresStall]   - preData[cpuId].nStall[OutsStoresStall]; 
    long long diffBranchStall       = thisData[cpuId].nStall[OutsBranchesStall] - preData[cpuId].nStall[OutsBranchesStall]; 
    long long diffReplayStall       = thisData[cpuId].nStall[ReplayStall]       - preData[cpuId].nStall[ReplayStall]; 
    long long diffPortConflictStall = thisData[cpuId].nStall[PortConflictStall] - preData[cpuId].nStall[PortConflictStall]; 
    long long diffSwitchStall       = thisData[cpuId].nStall[SwitchStall]       - preData[cpuId].nStall[SwitchStall]; 
    long long diffStall             = diffWinStall + diffROBStall + diffREGStall + diffLoadStall + diffStoreStall + diffBranchStall + diffPortConflictStall + diffSwitchStall;

    //print to result file
    fprintf(fp,"%d %lf %lf %lf %lf %lf %lf ",cpuId,(double)diffdl1miss/(double)interval,(double)diffil1miss/(double)interval,(double)diffl2miss/(double)interval,(double)diffdl1hit/(double)interval,(double)diffil1hit/(double)interval,(double)diffl2hit/(double)interval);
    
    fprintf(fp," %lf %lf %lf %lf %lf %lf ",(double)diffitlb/(double)interval,(double)diffdtlb/(double)interval,(double)diffStall/(double)interval,(double)diffNBranchHit/(double)interval,(double)diffNBranchMiss/(double)interval,(double)diffNBranchMiss/(double)(diffNBranchHit + diffNBranchMiss));

    fprintf(fp," %lld %lf %lf %lf %lf %lf %lf ",diffclock,diffIPC,L1MissRate,L2MissRate,diffenergy,diffpower,diffIPC/diffpower);
   
  statInst[threadId] += diffinst;
      
    if(statInst[threadId] > totalNInst){
         getTotStats(proc); 
    }


    //instruction   and  phase 
    long long diffiALU  = thisData[cpuId].nInst[iALU]   - preData[cpuId].nInst[iALU]; 
    long long diffiMult = thisData[cpuId].nInst[iMult]  - preData[cpuId].nInst[iMult];  
    long long diffiDiv  = thisData[cpuId].nInst[iDiv]   - preData[cpuId].nInst[iDiv]; 
    long long diffiBJ   = thisData[cpuId].nInst[iBJ]    - preData[cpuId].nInst[iBJ]; 
    long long diffiLoad = thisData[cpuId].nInst[iLoad]  - preData[cpuId].nInst[iLoad]; 
    long long diffiStore= thisData[cpuId].nInst[iStore] - preData[cpuId].nInst[iStore]; 
    long long difffpALU = thisData[cpuId].nInst[fpALU]  - preData[cpuId].nInst[fpALU]; 
    long long difffpMult= thisData[cpuId].nInst[fpMult] - preData[cpuId].nInst[fpMult]; 
    long long difffpDiv = thisData[cpuId].nInst[fpDiv]  - preData[cpuId].nInst[fpDiv]; 
    long long INT = diffiALU + diffiMult + diffiDiv;
    long long BJ  = diffiBJ;
    long long FP  = difffpALU + difffpMult + difffpDiv;
    long long MEM = diffiStore + diffiLoad;
    long long diffINT;
    long long diffBJ;
    long long diffFP;
    long long diffMEM;
    double    delt;
    int       tempid    = control_table[threadId][max_ID] + 1;
    int       currentid = control_table[threadId][current_ID];
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
			control_table[threadId][temp_NUM] = 0;
			phase_Z[0] = INT;
			phase_Z[1] = BJ;
			phase_Z[2] = FP;
			phase_Z[3] = MEM;
		}
		else {
			if (++control_table[threadId][temp_NUM] >= 4){	  
				control_table[threadId][temp_NUM] = 0;
				control_table[threadId][max_ID] = tempid;
				control_table[threadId][current_ID] = tempid;
				phase_table[threadId][tempid][0] = phase_Z[0];
				phase_table[threadId][tempid][1] = phase_Z[1];
				phase_table[threadId][tempid][2] = phase_Z[2];
				phase_table[threadId][tempid][3] = phase_Z[3];
			}
		}
	}
    }
    else{ //other phase
		diffINT = abs(phase_table[threadId][currentid][0] - INT); 
		diffBJ  = abs(phase_table[threadId][currentid][1] - BJ); 
		diffFP  = abs(phase_table[threadId][currentid][2] - FP); 
		diffMEM = abs(phase_table[threadId][currentid][3] - MEM); 
		delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;
//	printf("%lf\n", delt);
	if( delt >  ITV_diff){ //may be a new phase  // may a previous phase
		if(++control_table[threadId][temp_NUM] >= 4){// is another phase
			control_table[threadId][temp_NUM] = 0;
			for(int i = 0; i <= control_table[threadId][max_ID]; i++){ //Is previous phase?
				diffINT = abs(phase_table[threadId][i][0] - INT); 
				diffBJ  = abs(phase_table[threadId][i][1] - BJ); 
				diffFP  = abs(phase_table[threadId][i][2] - FP); 
				diffMEM = abs(phase_table[threadId][i][3] - MEM); 
				delt = (double)(diffINT + diffBJ + diffFP + diffMEM)/(double)interval;

				if(delt <= ITV_diff){  //a previous phase.
					control_table[threadId][max_ID]--;
					tempid = i;
					break;
				}
			}
			control_table[threadId][max_ID]++;
			control_table[threadId][current_ID] = tempid; //update current_ID
			phase_table[threadId][tempid][0] = INT;
			phase_table[threadId][tempid][1] = BJ;
			phase_table[threadId][tempid][2] = FP;
			phase_table[threadId][tempid][3] = MEM;

		}
	}
	else{ //is current
		control_table[threadId][temp_NUM] = 0;
	}
    }




    if(false && currentid != control_table[threadId][current_ID]){ //thread's phase is changed ,may need migrate
    	double C = diffIPC;
	double T = (double)INT/(double)interval;
	double B = (double)diffNBranchMiss/(double)interval;
	double M = (double)MEM/(double)interval;
	double S;
	double L;
	if(cpuId < 5){   //S core
		 S = 0.0407 + 0.1227*C - 0.0013 * T + 0.0110 * B - 0.0371 *M; //SS
		 L = 0.0631 + 0.0162*C + 0.0021 * T - 0.1284 * B - 0.0394 *M;  //SL
	}
	else{  //  Lcore
	
		 L = 0.0623 + 0.0040 * C - 0.0025 * T - 0.0905 * B - 0.0363 *M;  //LL
		 S = 0.0677 + 0.0012 * C - 0.0110 * T + 0.0028 * B - 0.0434 *M;  //LS
	}
        for(int i = 1; i <= 4; i++){
		phase_pw[threadId][tempid][i] = S;
		phase_pw[threadId][tempid][i + 4] = L;
	}
       if(true == is_Migrate(sigma_curr,sigma_dst)){
       		doMigrate(sigma_curr,sigma_dst);
       }	
    
    } 
       //fprintf(fp,"    %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld\n",diffiALU,diffiMult,diffiDiv,diffiBJ,diffiLoad,diffiStore,difffpALU,difffpMult,difffpDiv,control_table[cpuId][current_ID]);
       fprintf(fp,"    %lf %lf %lf %lf %lld\n",(double)INT/(double)interval,(double)BJ/(double)interval,(double)FP/(double)interval,(double)MEM/(double)interval,control_table[threadId][current_ID]);
    
    //save to the previous result
    preData[cpuId].dl1miss    =  thisData[cpuId].dl1miss;
    preData[cpuId].il1miss    =  thisData[cpuId].il1miss;
    preData[cpuId].l2miss     =  thisData[cpuId].l2miss;
    preData[cpuId].dl1hit     =  thisData[cpuId].dl1hit;
    preData[cpuId].il1hit     =  thisData[cpuId].il1hit;
    preData[cpuId].l2hit      =  thisData[cpuId].l2hit;
    preData[cpuId].itlb       =  thisData[cpuId].itlb;
    preData[cpuId].dtlb       =  thisData[cpuId].dtlb;
    preData[cpuId].nbranche   =  thisData[cpuId].nbranche;
    preData[cpuId].nbranchMiss=  thisData[cpuId].nbranchMiss;
    preData[cpuId].nbranchHit =  thisData[cpuId].nbranchHit;
    preData[cpuId].clock      =  thisData[cpuId].clock;
    preData[cpuId].sumInst    =  thisData[cpuId].sumInst;
    preData[cpuId].energy     =  thisData[cpuId].energy;
    
    for(int i = iOpInvalid; i < MaxInstType; i++){
	preData[cpuId].nInst[i] = thisData[cpuId].nInst[i];
    } 
    for(int i = NoStall + 1; i < MaxStall; i++){
	preData[cpuId].nStall[i] = thisData[cpuId].nStall[i];
    } 
    fclose(fp);
}
void xuStats::getTotStats(GProcessor *proc){
	//get app's whole statistical data 
	//such as IPC, Power ,IPC/Power etc.
      int cpuId       = 0;
      int threadId    = 0;
      long long clock = 0;
      double IPC      = 0.0;
      long long sumInst = 0;
      struct statDataContext currContext;
      clock = proc->getClockTicks();
      cpuId = proc->getId();
      threadId = arc_sigma[cpuId];
      
      if(clock > 0){
   	   //clock = proc->getClockTicks();   // clock 

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
  //    printf("clock = %ld\n",clock);
      	   
	   currContext.clock   = clock;
      	   currContext.sumInst = sumInst;
      	   currContext.IPC     = IPC;
      	   currContext.power   = corePower;
      	   currContext.energy  = coreEnergy;
     	   currContext.pw      = IPC/corePower;

	   fprintf(totFp,"%d ",ThreadId);
	   fprintf(totFp,"%ld ",sumInst - context[threadId].sumInst);
	   fprintf(totFp,"%ld ",clock - context[threadId].clock);
	   fprintf(totFp,"%lf",IPC -context[threadId].IPC);
	   fprintf(totFp,"%lf",corePower -context[threadId].power);
	   fprintf(totFp,"%lf",coreEnergy - context[threadId].energy);
	   fprintf(totFp,"%lf",coreIPC/corePower -context[threadId].pw);
	  // fprintf(totFp,"%lf %lf %lf %lf\n",corePower,coreEnergy,IPC/corePower);
		
	   context[cpuId] = currContext;
	   sigma[threadId]  = 0;
	   arc_sigma[cpuId] = 0;
      	   fclose(totFp);
       }
}



bool xuStats::is_Migrate(int *curr, int *dst){
     double * matrix[xu_nCPU];
     for(int i = 1; i < xu_nThread; i++){
     	//i is thread's id
	int phaseid = control_table[i][current_ID];  
	matrix[i] = phase_pw[i][phaseid];
     }
    double max = 0.0; 
    int A[xu_nPhase] = {0,1,2,3,4,5,6,7,8};
    dfs(matrix,dst,A,max,1,xu_nThread); 
    for(int i = 1; i < xu_nPhase; i++){
    	if(curr[i] != dst[i]) return true;  
    }
    return false;
    // matrix[9][9]:  1~8  1~8 is useful , 0 raw and 0 column is invalid
}



void xuStats::dfs(double **matrix, int *dst,int *A,double &max,int start, int end){

	if(start == end){
		double sum = 0.0; 
		for(int i= 1; i< xu_nPhase; i ++){
			sum += matrix[i][A[i]];
		}
		if(sum > max){
			max = sum;
			for(int i = 1; i < xu_nPhase; i++){
				dst[i] = A[i];
			}
		}
		return;
	}
	else{
		for(int i = start; i < end; i++){
			swap((A+i),(A+start));
			dfs(matrix,dst,A,max,start + 1, end);
			swap((A+i),(A+start));
		}
	}
}

void xuStats::doMigrate(int *curr, int *dst){     //migrate threads from curr to dst.

	int        cpuid;
	GProcessor *currentCPU;
	ProcessId  *proc;
	
	for(int pid = 1; pid < xu_nThread ; pid++){  //switch out
		if(curr[pid] != dst[pid]){ //thread i need to be migrated form core_curr[i] to core_dst[i]
			cpuid      = curr[pid];
			currentCPU = osSim->id2GProcessor(cpuid);
			proc       = ProcessId::getProcessId(pid);
			osSim->cpus.switchOut(cpuid, proc); //switch out proc
		
		}
	
	}
	for(int pid = 1; pid < xu_nThread ; pid++){  //switch in
		if(curr[pid] != dst[pid]){      //thread i need to be migrated form core_curr[i] to core_dst[i]
			curr[pid] = dst[pid];   // update thread->cpu
			cpuid = curr[pid];
			currentCPU = osSim->id2GProcessor(cpuid);
			proc       = ProcessId::getProcessId(pid);
			osSim->cpus.switchIn(curr[pid],proc);
			arc_sigma[cpuid] = pid;  // update cpu -> thread
		
		}

	}
}

