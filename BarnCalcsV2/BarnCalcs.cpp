#include <stdio.h>
#include <math.h>
#include <stdarg.h>

/* 
 BARN DOOR TRACKER, ISOLECES TYPE, CALCULATOR
 2014 ZZJ
 */

// undefine the normal double precision functions...
#undef PI
#undef sin
#undef cos
#undef pow
#undef sqrt
#undef round
#undef abs

// ... and alias them to the long double functions. So, i'm lazy.
#define sin sinl
#define cos cosl
#define pow powl
#define sqrt sqrtl
#define round roundl
#define acos acosl
#define abs fabsl

// constants
const long double PI = 3.141592653589793238462643383279L;
const long double RAD2DEG = (180.0L / PI);
const long double DEG2RAD = (PI / 180.0L);

const long double ARCSEC_RAD = RAD2DEG * 3600; // radians to arcseconds

const long double IN2CM = 2.54;
const long double CM2IN = 0.393701L;

long double angle(long double C);      // angle given length of drive screw
long double ddt(long double t);        // ideal d/dt in length/time at time t

// relics of quadmath
void printQ(long double num); // print long double float
void printQ(const char * st, long double num); // print prefix and long double
void printQ(long double num, const char * ed); // print long double and suffix
void printQ(const char * st, long double num, const char *fi); // print prefix, long double, and suffix

void logprintf(FILE *f, const char *fmt, ...); // print string with format and write it to file f if f is non-null

//////////////////////////////////////////////////////
/////////// INPUT VALUES /////////////////////////////
//////////////////////////////////////////////////////

// L = long double constant
const long double A = 30.006L; // arm (the moving bit) of mount
const long double B = 30.406L; // base of mount

const long double Cinitial = 6.507L; // length of drive rod at starting position

const long double V = (2*PI) / (23*3600+56*60+4); // target velocity in rad/s
// one sidereal day is 23 hrs, 56 minutes, and 4 seconds
// the time in which the stars go one full revolution

const long double THREADS_PER_CM = 20.0L /* per in */ / IN2CM;
//const long double THREADS_PER_CM = 7.93; // drive rod threads/cm measured

// number of steps between interval change
const long double UPDATE_INTERVAL = 256.0L;
// 256 - use an single byte value, increment each step, update if = 0

const int MINIMUM_ADVANCE  = 0;   // minimum number of steps until next correction - smooths things out
const int ADJUSTMENT_AMOUNT = 1;  // increment to adjust in

const int MAX_RUNTIME = 70 * 60;// the lower of these two values will be used

// stepper parameters
const long double DEGREES_PER_STEP = 7.5L; // in degrees
const long double MICROSTEP_DIVISOR = 4;   // 1 for no microstepping
const long double GEAR_RATIO = 55;         // reduction (if any, otherwise use 1)

const long double TIME_UNITS_PER_SECOND = 1000000; // microseconds

#define TABLE_FILENAME "output.tsv"
#define OUTPUT_FILENAME "table.h"

//////////////////////////////////////////////////////
/////////// CALCULATED VALUES ////////////////////////
//////////////////////////////////////////////////////

const long double S = angle(Cinitial);    /* = starting angle, in radians */

const long double STEPS_PER_REV = (360/(DEGREES_PER_STEP/MICROSTEP_DIVISOR)) * GEAR_RATIO; // 55 = gear ratio, 192 = steps per stepper rev

const long double CM_PER_STEP = (1.0L/STEPS_PER_REV)*(1/THREADS_PER_CM);

//////////////////////////////////////////////////////
/////////// MATH FUNCTIONS ///////////////////////////
//////////////////////////////////////////////////////

// angle given side length C
long double angle(long double C) {
    return acos((pow(A,2) + pow(B,2) - pow(C,2)) / (2*A*B));
}

// rate of change at time t. Mind your unit of time!
// derivative of law of cosines SAS case
long double ddt(long double t) {
    return (A*B*V*sin(t*V+S))/
    (sqrt(-2*A*B*cos(t*V+S)+pow(A,2)+pow(B,2)));
}

//////////////////////////////////////////////////////
/////////// MAIN CODE ////////////////////////////////
//////////////////////////////////////////////////////

// just some info
void digest(long double t) {
    printQ("At t=",t,"seconds, d/dt is:");
    printQ(" ", ddt(t)," cm/sec");
    printQ(" ", ddt(t) * 60," cm/min");
    printQ(" ", ddt(t) / CM_PER_STEP,"steps/sec");
	printQ(" ", ddt(t) / CM_PER_STEP*60,"steps/min");
    printQ(" ", (ddt(t) / CM_PER_STEP*60) / STEPS_PER_REV," RPM");
    printQ(" ", TIME_UNITS_PER_SECOND / (ddt(t) / CM_PER_STEP),"uSec/step");
    printQ(" ", 1000 / (ddt(t) / CM_PER_STEP) * UPDATE_INTERVAL,"ms/interval");
    printf("\n");
}

int main (int argc, char**argv) {
    printQ("MAX_RUNTIME=",MAX_RUNTIME/60,"minutes");
    printQ("THREADS_PER_CM=",THREADS_PER_CM);
    printQ("CM_PER_STEP=",CM_PER_STEP,"cm");
    printQ("STEPS_PER_REV=",STEPS_PER_REV);
    printQ("A=",A,"cm");
    printQ("B=",B,"cm");
    printQ("Cinitial=",Cinitial,"cm");
    printQ("V=",V,"rad/sec");
    printQ("  ",V*RAD2DEG * 60.0L,"deg/min");
    printQ("  ",V*RAD2DEG * 60.0L *60,"deg/hr");
    
    printQ("S=",S,"rad");
    printQ("  ",S*RAD2DEG,"deg");
    printf("\n");
    
    digest(0);
    digest(MAX_RUNTIME);
    
    // delay to use at t=0
    int delay = round(TIME_UNITS_PER_SECOND / (ddt(0) / CM_PER_STEP));
    int delay_base = delay;
    
    // current time
    long double time = 0;
    
    // statistics variables
    int updateCount = 0;
    long double errorAccum = 0;
    long double maxError = 0;
    
    int num_updates = 65535; // max number of updates to calculate, will be limited by time variable
    int offsets[num_updates];
    
    int advance = 0;
    
    FILE* file = fopen(TABLE_FILENAME,"w");
    
    if (file)
        printf("*** Logging table to " TABLE_FILENAME "\n\n");
    
    // print out a nice header
    // this can easily be imported into excel (tab delimited)
    
    logprintf(file,"Change\tTime (sec)\tDelay(uSec)\tAngle(deg)\tC length(cm)\tError(arcsec)\t\n");
    
    for (int i = 1; i < num_updates; i++) { // groups of rollover_steps
        
        long double length = Cinitial + i * UPDATE_INTERVAL * CM_PER_STEP;
        
        long double angle_now = angle(length);
        
        if (time >= MAX_RUNTIME) { // if we're above the max runtime, print final line and exit
            logprintf(file,"END\t%f\t\t%1.20LF\t%1.20LF\t\n",(double)time,angle_now*RAD2DEG,length);
            num_updates = i;
            break;
        }
        
        long double time_current =time + (UPDATE_INTERVAL * delay)/TIME_UNITS_PER_SECOND;
        long double time_faster = time + (UPDATE_INTERVAL * (delay-ADJUSTMENT_AMOUNT))/TIME_UNITS_PER_SECOND;
        long double time_slower = time + (UPDATE_INTERVAL * (delay+ADJUSTMENT_AMOUNT))/TIME_UNITS_PER_SECOND;

        long double ideal_angle_current = S+V*time_current;
        long double ideal_angle_faster =  S+V*time_faster;
        long double ideal_angle_slower =  S+V*time_slower;
        
        // calculate one slower and one faster error, to see if it's worth speeding up or slowing down
        long double error_current = (angle_now - ideal_angle_current)*ARCSEC_RAD;
        long double error_faster = (angle_now - ideal_angle_faster)*ARCSEC_RAD;
        long double error_slower = (angle_now - ideal_angle_slower)*ARCSEC_RAD;
        
        long double selected_error = error_current;
        
        // positive error: too fast
        // negative error: too slow
        
        if (advance >= MINIMUM_ADVANCE) { // value to remove oscillations
            if (error_current > 0 && error_slower < error_current && error_slower > 0) {
                // going too fast; slowing down would reduce error. Do it.
                printf("-");
                delay += ADJUSTMENT_AMOUNT;
                advance = 0;
                selected_error = error_slower;
                
            } else if (error_current < 0 && error_faster > error_current && error_faster < 0) {
                // going too slow; speeding up would reduce error. Do it.
                printf("+");
                delay -= ADJUSTMENT_AMOUNT;
                advance = 0;
                selected_error = error_faster;
            }
        } else
            advance++;
        
        // print and log line
        logprintf(file, "\t%f\t%d\t%1.20LF\t%1.20LF\t%1.20LF\n",(double)time,delay,angle_now*RAD2DEG,length,selected_error);
        
        // save the offset and apply it to the time elapsed
        offsets[i] = delay - delay_base;
        time = time + (UPDATE_INTERVAL * delay)/TIME_UNITS_PER_SECOND;
        
        if (abs(selected_error) > abs(maxError))
            maxError = selected_error;
        
        errorAccum += abs(selected_error);
        updateCount++;
    }
    
    logprintf(file,"\n");
    
    if (file)
        fclose(file);
    
    // print out the table
  //  printf("const PROGMEM uint16_t table[%d] = { ",num_updates-2);
    
   // raw table of delays
    /*for (int i = 2; i < num_updates; i++)
        printf("%d%c",offsets[i] + delay_base,((i+1)!=num_updates?',':'}'));
    */
   // printf(";\n");
   /*
    // table of offsets
           printf("\n");
    printf("const PROGMEM uint8_t offset_table[%d] = { ",num_updates-2);
    
    for (int i = 2; i < num_updates; i++)
        printf("%d%c",offsets[i],((i+1)!=num_updates?',':'}'));
    
    printf(";\n// update interval= %d steps\nconst uint16_t baseval = %d - 100;\n\n",(int)UPDATE_INTERVAL,delay_base);*/
    
    // table of change at interval
   // printf("const PROGMEM uint8_t changes_table[] = { ",num_updates-2);
    
    // calculate the change points
    int count = 0;
    int i = 2;
    delay = offsets[1];
    
    int changes[num_updates];
    int changeCount = 0;
    
    while(i < num_updates) {
        int offset = offsets[i]-delay;
        delay = offsets[i];
        changes[i] = offset;
        if (offset != 0)
            changeCount ++;
        
        //printf("%d,",changes[i]);
        i++;
    }
    
  //  printf("\n\n");
    
    i = 2;
    int c = 0;
    count = 0;
    
    // print table of encoded changes
    // byte format:
    // bit: 7 8 6 5 4 3 2 1 0
    //      [  interval   ] C
    // interval: nr of intervals until next update
    // C: 1 add one to delay, 0 subtract one from delay
    
    // header
    
    file = fopen(OUTPUT_FILENAME,"w");
    
    if (file)
        printf("*** Writing offset table to " OUTPUT_FILENAME "\n\n");
    
    logprintf(file,"#ifndef TABLE_H\n#define TABLE_H\n\n#ifndef DELAY_TWEAK\n#define DELAY_TWEAK 0\n#endif\n\n// table generated by BarnCalcs\n// A=%LF, B=%LF, Cinitial=%LF\n// %d steps/rev, %LF threads/cm\n// interval: %d steps\n// runtime: %f minutes\n",A,B,Cinitial,(int)STEPS_PER_REV, THREADS_PER_CM,(int)UPDATE_INTERVAL,(double)(time/60));
    
    logprintf(file,"const PROGMEM uint8_t offset_table[%d] = { ",changeCount);
    
    while (changes[i] == 0)
        i++;
    
    int initial = i - 1; // number of initial values where no adjustment is required
    
    while(i < num_updates) {
        int chg = changes[i] > 0 ? 1 : 0;
        c = 1;
        
        while (changes[i+c] == 0 && (i+c) < num_updates)
            c++;
        
        if (c > 127) // TODO: encode as two (or more) values instead
            logprintf(file,"\n\nERROR: c > 127 not implemented....\n\n");
        
        logprintf(file,"%d%c", chg | (c << 1),((i+c)!=num_updates)?',':' ');
       // printf("%c  skip %d: %d\n",chg ? '+' : '-',c-1,chg | (c << 1));
        i += c;
    }
    
    logprintf(file,"};\n\nconst uint8_t first_update = %02d;     // num intervals to first change\nconst uint16_t baseval = %d - 100 - (DELAY_TWEAK); // start value - 100us pulse high time\n\n#endif\n\n",initial,delay_base);
    
    if (file) fclose(file);
    
    int num_steps = (UPDATE_INTERVAL * ((double)num_updates-1));
    
    printf("### Statistics ###\n\nTime: %f minutes\nIntervals: %d\nSteps: %d\nRevs: %LF\nDistance: %LF cm\n\n",(double)(time/60),num_updates-1,num_steps,((double)num_steps)/STEPS_PER_REV,((double)num_steps)/STEPS_PER_REV/THREADS_PER_CM);
    
    printQ("Avg error: ", errorAccum / updateCount,"arcsec");
    printQ("Max error: ",maxError,"arcsec");
    
    if (maxError > 0.1)
        printf("\nWARNING: Error is high. Probably ADJUSTMENT_AMOUNT (currently %d) is too small.\n\n",ADJUSTMENT_AMOUNT);
    
  	return 0;
}

/*
 To reconstitute:
 
 const uint8_t offset_table[] = { ... };

 const uint8_t first_update = (value goes here);
 const uint16_t baseval = (value goes here);
 
 /*int skip;
 
 int count = 0;
 for (int i = 0; i < sizeof(offset_table); i++) {
 if (offset_table[i] == 0) {
 printf("%d,",baseval);
 count++;
 continue;
 }
 
 baseval += (offset_table[i] & 1) ? 1 : -1;
 skip = offset_table[i] >> 1;
 
 for (int cnt = 0; cnt < skip; cnt++) {
 printf("%d,", baseval);
 count++;
 }
 
 
 }

 */

/* these are relics of using quadword precision .... too lazy to replace */
void printQ(long double num) {
    printf("%1.15LF",num);
}

void printQ(const char * st, long double num) {
    printf("%s ",st);
    printQ(num);
    printf("\n");
}

void printQ(long double num, const char * ed) {
    printQ(num);
    printf(" %s\n",ed);
}

void printQ(const char * st, long double num, const char *fi) {
    printf("%s ",st);
    printQ(num);
    printf(" %s\n",fi);
}


void logprintf(FILE *f, const char *fmt, ...) {
    va_list argptr;
    va_start(argptr, fmt);
    
    char buffer[256];
    vsnprintf(buffer,256,fmt,argptr);
    
    va_end(argptr);
    
    if (f)
        fprintf(f, "%s",buffer);
    
    printf("%s",buffer);
  
}

