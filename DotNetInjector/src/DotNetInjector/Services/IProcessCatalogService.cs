using DotNetInjector.Models;

namespace DotNetInjector.Services;

public interface IProcessCatalogService
{
    Task<IReadOnlyList<TargetProcessInfo>> GetProcessesAsync(CancellationToken cancellationToken = default);
}