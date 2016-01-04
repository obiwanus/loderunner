#include "loderunner.h"

const int kLevelCount = 3;

const char *LEVELS[kLevelCount] = {

    "      r          r       | r      \n"
    "                         |        \n"
    "                         |        \n"
    "  e t                    |        \n"
    "  =++=++=+++=#======+    |        \n"
    "             #-----------|        \n"
    "             #    ==#    |        \n"
    "             #  e ==#    | t      \n"
    "             #    ==#   =====#====\n"
    "             #    ==#        #\n"
    "             #    ==#        #\n"
    "             #    ==#       t#\n"
    "===#==========    ========#=======\n"
    "   #                      #\n"
    "   #                      #\n"
    "   #                      #\n"
    "==============#===========#\n"
    "              #           #\n"
    "              #           #\n"
    "          e t #-----------#  t e\n"
    "      #=========          =======#\n"
    "      #                          #\n"
    "      #          p  t            #\n"
    "==================================",

    "+++==+=++=++++=+++===|=+===++=+=\n"
    "=      r       r    =E   r     =\n"
    "= t                 =|   t     =\n"
    "=====#====#    t    =|========#=\n"
    "=====#================= ====  #=\n"
    "=    #  t e      ====== ====  #=\n"
    "=#=========#=========== ====  #=\n"
    "=#   ======#=======tt== ====  #=\n"
    "=#   ======#=========== ==== t#=\n"
    "=#   ======#======    ========#=\n"
    "=#   =     #                 =#=\n"
    "=#   =    t#    t     e   t  =#=\n"
    "=#   ==#========#===========#=#=\n"
    "=#   ==#        #===========#=#=\n"
    "=#  t==#  p    # =======    #=#=\n"
    "=#==== #      #     ====    #=#=\n"
    "=#==== #    #===========    #=#=\n"
    "=#==== #    #===========    #=#=\n"
    "=#====#=====#======t====t t #=#=\n"
    "=#====#=====#=================#=\n"
    "=# e  #==t==#     t    e      #=\n"
    "================================",

    "================================\n"
    "=  t=t= t =t=t= =t= =t=t=t =t  =\n"
    "=   = =   = = = = = = = =  =   =\n"
    "=   = =   = = = = = = = =  =   =\n"
    "=  t=t=t t=t=t= =t=t=t=t=t =t  =\n"
    "=   = =   = = = = = = = =  =   =\n"
    "=   = =   = = = = = = = =  =   =\n"
    "=  t=t=t t=t=t= =t=t=t=t=tt=t  =\n"
    "=   = =   = = = = = = = =  =   =\n"
    "=   = =   = = = = = = = =  =   =\n"
    "=   t  t t t t   t t t t t tt  =\n"
    "=                              =\n"
    "=                              =\n"
    "=   t  t t t t   t t t t t  t  =\n"
    "=                              =\n"
    "=                              =\n"
    "=   t       t     t t  t t  t  =\n"
    "=       d                      =\n"
    "=       d                      =\n"
    "=  d d  d                 d    =\n"
    "=  d d  d                 dd   =\n"
    "=  d d dtd d d   d d d    dd   =\n"
    "=dddddddddddddddddddddddddddddd=\n"
    "=                              =\n"
    "================================",
};