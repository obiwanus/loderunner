#include "loderunner.h"

const int kLevelCount = 14;  // the last one is the you win screen

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

    "   t     r       r             #\n"
    "#++=++#                        #\n"
    "#     #               t        #\n"
    "#     #      #===========#     #\n"
    "# t e #      #           # t   #\n"
    "#=+=+=#      #           #====f#\n"
    "#     #---   #           #     |\n"
    "#     #      #           #     |\n"
    "#     #------#-------   e#     |\n"
    "#     #      #      #====++++++#\n"
    "#     #      #      #          #\n"
    "#     #      #  t   #          #\n"
    "#   e # t    #======#          #\n"
    "++===+=++==++#                 #\n"
    "+===++       #          #====#==\n"
    "+   ++       #          #    #  \n"
    "+t  ++       #   -------#    # t\n"
    "=========#====++++      #   ====\n"
    "         #              #\n"
    "         #              #\n"
    "         #     p        #\n"
    "================================",

    "    r                r         |\n"
    "------------     t             |\n"
    "#          #=============#     |\n"
    "#          #             #     |\n"
    "# t        #             #++++++\n"
    "======#    #             #      \n"
    "      #  e #       t     #      \n"
    "      #======#=======#====      \n"
    "      #      #       #    --    \n"
    "      #      #       #      --  \n"
    "   t  #      #   e   #        --\n"
    "=====#=   #=======#===          \n"
    "     #    #       #            t\n"
    "     #----#       #  e         f\n"
    "     #       #==========#       \n"
    "     #       #==========#       \n"
    "     #       #==========#       \n"
    "====#==========     t  =======#=\n"
    "====#========== #====# =======#=\n"
    "    #           #====#        # \n"
    "    #        p  #====#    t   # \n"
    "================================",

    "|       r               r       \n"
    "|-------------                  \n"
    "#       #  r  = t =  r  #       \n"
    "#      ###    =====    ###      \n"
    "#   t   #   t       t   #   t   \n"
    "#   ##  #  ##   r   ##  #  ##   \n"
    "#   # ##### #       # ##### #   \n"
    "#   # ##### #   #   # ##### #   \n"
    "#   #  tet  #  ###  #  tet  #   \n"
    "#    #=====#    #    #=====#    \n"
    "#     ##### ##  #  ## #####     \n"
    "#           # ##### #           \n"
    "#           # ##### #           \n"
    "#           # ##### #           \n"
    "#           #  tet  #           \n"
    "#     t      #=====#      t     \n"
    "#=======#     #####     #=======\n"
    "#       #               #       \n"
    "#       #               #       \n"
    "#       #               #       \n"
    "#       #       t  p    #       \n"
    "================================",

    "     r      |         r     r   \n"
    "            |               e   \n"
    "======      |      ====#========\n"
    "    ===     |     ===  #        \n"
    "     ===    |    ===   #        \n"
    "      ===   |  t===    #        \n"
    "t e   ====  |  ====    #    t   \n"
    "===#======= | =====#============\n"
    "   #    ====#====  #            \n"
    "   #        #      #            \n"
    "   #        #      #            \n"
    "   #te      #      #     t      \n"
    "#====#      #     ====#===      \n"
    "#    #      #         #         \n"
    "#    #                #         \n"
    "#    #                #         \n"
    "#    #      t      e  #         \n"
    "#    #========#=============#===\n"
    "#             #             #   \n"
    "#             #             #   \n"
    "#             #   p         #   \n"
    "================================",

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
    "=#==f= #      #     ====    #=#=\n"
    "=#==f= #    #===========    #=#=\n"
    "=#==f= #    #===========    #=#=\n"
    "=#==f=#=====#===========t t #=#=\n"
    "=#==f=#=====#======t==========#=\n"
    "=# e  #==t==#     tT   e      #=\n"
    "================================",

    "|       r  r      |  r          \n"
    "|                 |     --------\n"
    "|               e |     #      #\n"
    "=====#==    =#=== |    e#    t #\n"
    "     #       #    | ====#=======\n"
    "     #       #    |     #       \n"
    "     #       #    |     #       \n"
    "  e  #    t  # |  |     #  f    \n"
    "=====#=====  # |  |     #  t    \n"
    "     #       # ||||     #  f    \n"
    "     #       #          #       \n"
    "     #       #          #       \n"
    "     #       #          #       \n"
    "  t  #    t  #          #       \n"
    "==#==========#====#     #       \n"
    "  #               # t   # ==== #\n"
    "  #               #-----# ==== #\n"
    "  #   t      f          # =t = #\n"
    "========#    t          # =====#\n"
    "        #    f   =t=    #      #\n"
    "        #         T     #      #\n"
    "        # p           t #  e   #\n"
    "================================",

    " r           |r   |           r \n"
    "             |    |             \n"
    "   t       #=|    |=#       t   \n"
    "#=====#----#=|    |=#----#=====#\n"
    "#=   =#    #=|    |=#    #=   =#\n"
    "#=   =#     =|    |=     #=   =#\n"
    "#=   =#     =|    |=     #=   =#\n"
    "#= t =#     =|    |=     #= t =#\n"
    "#=====#     =|    |=     #=====#\n"
    "#=   =#     =|    |=     #=   =#\n"
    "#=   =#     =|    |=     #=   =#\n"
    "#=   =#     =#====#=     #=   =#\n"
    "#=   =#-----=#    #=-----#=   =#\n"
    "#=   =     #=#    #=#     =   =#\n"
    "#=et =     #=#  te#=#     = t =#\n"
    "#=ff==     #=++++++=#     ==f==#\n"
    "#    =f    #        #    f=    #\n"
    "#     f=   #        #   =f     #\n"
    "#      ==e #    p   #  ==      #\n"
    "================================",

    "|         r                 r   \n"
    "|-------------------------------\n"
    "#= e   =                 =     =\n"
    "# =====                   =====#\n"
    "# = ===                   =====#\n"
    "# =====                   =====#\n"
    "# === =  e     e    e     === =#\n"
    "# ===========================#=#\n"
    "# ===========t === t=========#=#\n"
    "# ===========================#=#\n"
    "# ===========================#=#\n"
    "# ============     ==========#=#\n"
    "# ============     ==========#=#\n"
    "# ============     ==========#=#\n"
    "# ============     ==========#=#\n"
    "# ============     ==========#=#\n"
    "# ============    t#=========#=#\n"
    "#             =====#---------# #\n"
    "#            ++=+==#           #\n"
    "#            +=++++            #\n"
    "#       p   +===+=      e      #",

    "  r             r          | r  \n"
    "           t              |     \n"
    "         #========  t    |      \n"
    "    t    #       ====== |       \n"
    "===========#       ||||||       \n"
    "           #       |   t        \n"
    "           #=======|=======#==  \n"
    "           #       |       #    \n"
    "e          #       |       #    \n"
    "===========#       |  e    #   t\n"
    "===========#========+=+    #====\n"
    "===========#               #    \n"
    "===========#               #    \n"
    "==      ===#         t ----#    \n"
    "==      ===#         ff    #    \n"
    "==      ===#               #    \n"
    "==      ===#               #    \n"
    "==  tt  ===#       t       #    \n"
    "============#=========#    #    \n"
    "            #         =========#\n"
    "            #                  #\n"
    "    t     p #                e #\n"
    "================================",

    "   r      #     r            r  \n"
    "          #                     \n"
    "          #                     \n"
    " et       #                 te  \n"
    "#=== et   #             te  ===#\n"
    "#   ====  #====|#====#  ====   #\n"
    "#               #t   #         #\n"
    "#               ##   #         #\n"
    "#              t#    #         #\n"
    "#              ##    #         #\n"
    "#               #t             #\n"
    "#              t##             #\n"
    "#              ##              #\n"
    "#               #t             #\n"
    "#              t##             #\n"
    "#              ##              #\n"
    "#              ##t             #\n"
    "#               ##             #\n"
    "#              t#              #\n"
    "#              ##t             #\n"
    "#               ##             #\n"
    "#          p   t#t             #\n"
    "#       #==============#       #",

    "    r            r          | r \n"
    "      t       t             |   \n"
    "#====================#      #   \n"
    "#                t   #------#   \n"
    "======================      #   \n"
    "==========#                 #  t\n"
    "=========##                 #===\n"
    "==    ==###--------#        #   \n"
    "==t  t==##         #        #   \n"
    "=======##          #        #   \n"
    "=======##       e  # t   e  #  t\n"
    "======##     #=====+++++====#===\n"
    "======##     #              #   \n"
    "      #      #              #   \n"
    "      #      #              #   \n"
    "      #      #--------------#   \n"
    "     e#   t  #              #  t\n"
    "#============# t          t #===\n"
    "#            # ============ #   \n"
    "#            #              #   \n"
    "#            #              #   \n"
    "#      t     #    p    t    #   \n"
    "================================",

    "    r            r            r|\n"
    "               e      e        |\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "=t#==# #=#t#=# #=#t#=# #=#t=#==|\n"
    "=t#==# #=#t#=# #=#t#=# #=#t=#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "==#==# #=#=#=# #=#=#=# #=#==#==|\n"
    "= #==#t#=# #=#t#=# #=#t#=# =#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "==#==#=#=#=#=#=#=#=#=#=#=#==#==|\n"
    "t #  # # # # # # # # # # #  #==|\n"
    "#==============================|\n"
    "#                              |\n"
    "#             te t t  e        |\n"
    "#++++++++++++++++++++++++++++++|\n"
    "#                              |\n"
    "#t      p                     t \n"
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