#pragma once

#include <HydraCore/Common/Platform.h>
#include <HydraCore/Common/Types.h>
#include <HydraCore/Configuration/ConfigSchema.h>   // for ValidationResult

namespace Hydra {

class Configuration;

// =============================================================================
// IHotReloadListener  (interface only — no file-system watching in this module)
//
// Register implementations with ConfigurationManager::AddReloadListener().
// The manager calls these callbacks whenever ConfigurationManager::Reload()
// completes a successful or failed reload cycle.
//
// Lifetime contract:
//   The listener must outlive the ConfigurationManager (or be removed via
//   RemoveReloadListener() before destruction).  The manager holds raw
//   (non-owning) pointers.
// =============================================================================

class HYDRA_API IHotReloadListener
{
public:
    virtual ~IHotReloadListener() = default;

    // -------------------------------------------------------------------------
    // Successful reload
    // -------------------------------------------------------------------------

    /// Called after a successful reload and (optional) schema validation.
    ///
    /// @param sourcePath  Path of the primary configuration file that changed.
    /// @param newConfig   The freshly loaded, merged Configuration.
    ///
    /// @return  Return true to accept the new configuration.
    ///          Return false to reject it — ConfigurationManager will keep the
    ///          previous configuration and call OnConfigReloadRejected().
    virtual bool OnConfigReloaded(StringView         sourcePath,
                                  const Configuration& newConfig) = 0;

    // -------------------------------------------------------------------------
    // Failed reload  (default implementations — override as needed)
    // -------------------------------------------------------------------------

    /// Called when the reload fails schema validation.
    ///
    /// @param sourcePath  Path of the file that triggered the reload.
    /// @param result      Validation result carrying all error messages.
    virtual void OnConfigReloadFailed(StringView               sourcePath,
                                      const ValidationResult&  result)
    {
        (void)sourcePath;
        (void)result;
    }

    /// Called when a listener returned false from OnConfigReloaded().
    ///
    /// @param sourcePath  Path of the file whose reload was rejected.
    virtual void OnConfigReloadRejected(StringView sourcePath)
    {
        (void)sourcePath;
    }
};

} // namespace Hydra
