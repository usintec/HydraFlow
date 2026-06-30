#include <HydraCore/Application/EngineContext.h>
#include <HydraCore/Config/ConfigManager.h>

namespace Hydra {

ConfigManager& EngineContext::GetConfig() const
{
    return RequireService<ConfigManager>();
}

} // namespace Hydra
