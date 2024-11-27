//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#include "Frameworks/Canvas/Shaders/BlitShader.h"


IMPLEMENT_SHADER_TYPE(template<>, TDABlitShader<EDABlurShaderOp::Add>, TEXT("/Plugin/DungeonArchitect/Canvas/Utils/BlitShader.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TDABlitShader<EDABlurShaderOp::Multiply>, TEXT("/Plugin/DungeonArchitect/Canvas/Utils/BlitShader.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TDABlitShader<EDABlurShaderOp::Copy>, TEXT("/Plugin/DungeonArchitect/Canvas/Utils/BlitShader.usf"), TEXT("MainCS"), SF_Compute);


