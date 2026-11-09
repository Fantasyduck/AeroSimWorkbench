int compileAccessResultHeader();
int compileConfigurationModelsHeader();
int compileDataAccessHeader();
int compileEnumTokensHeader();
int compileExecutionModelsHeader();
int compileIntegrationPortsHeader();
int compileMeshModelsHeader();
int compilePostProcessRefsHeader();
int compileReferencesHeader();
int compileResourceModelsHeader();
int compileResultModelsHeader();
int compileTaskModelsHeader();
int compileTaskPortsHeader();
int compileTelemetryHeader();
int compileWorkflowModelsHeader();

int main()
{
    return compileAccessResultHeader()
        + compileConfigurationModelsHeader()
        + compileDataAccessHeader()
        + compileEnumTokensHeader()
        + compileExecutionModelsHeader()
        + compileIntegrationPortsHeader()
        + compileMeshModelsHeader()
        + compilePostProcessRefsHeader()
        + compileReferencesHeader()
        + compileResourceModelsHeader()
        + compileResultModelsHeader()
        + compileTaskModelsHeader()
        + compileTaskPortsHeader()
        + compileTelemetryHeader()
        + compileWorkflowModelsHeader();
}

