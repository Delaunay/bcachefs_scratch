# better way ?
INCLUDE_DIRECTORIES(.)

# Source files
# ====================================
#           (Glob is not recommended)
#   file(GLOB APL_SRC *.cc)
#   file(GLOB_RECURSE APL_SRC *.cc)

SET(BCACHEFS_SCRATCH_HDS
    bcachefs.h
    logger.h
)

SET(BCACHEFS_SCRATCH_SRC
    bcachefs.cpp
    main.cpp
    logger.cpp
)

# main library (prevent recompilation when building tests)
ADD_LIBRARY(bcachefs ${BCACHEFS_SCRATCH_HDS} ${BCACHEFS_SCRATCH_SRC})
TARGET_LINK_LIBRARIES(bcachefs spdlog::spdlog)

#  main executable
# ==========================

ADD_EXECUTABLE(main main.cpp)
TARGET_LINK_LIBRARIES(main spdlog::spdlog bcachefs)
