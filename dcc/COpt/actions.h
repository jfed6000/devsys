/* Action Instruction Strings */

typedef struct ins {
    struct ins *nxtins;
    char *mnp,*opp;
} instr;

typedef struct act {
    struct act *nxtact;
    instr *actp,*repp;
    struct act *lskip;  /* devsys: the first action after this one's
                         * run of actions with the same lastc */
    char lastc;     /* devsys: the last character of actp's operand
                     * pattern if it is an ordinary one, else 0 (optim) */
} action;

/*
PUSH ORDER: pc,u/s,y,x,dp,{b,a|d},cc
PULL ORDER: cc,{d|a,b},dp,x,y,u/s,pc
*/
