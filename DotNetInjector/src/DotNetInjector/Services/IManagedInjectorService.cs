using DotNetInjector.Models;

namespace DotNetInjector.Services;

public interface IManagedInjectorService
{
    Task<InjectionExecutionResult> ExecuteAsync(ManagedInjectionRequest request, CancellationToken cancellationToken = default);
}