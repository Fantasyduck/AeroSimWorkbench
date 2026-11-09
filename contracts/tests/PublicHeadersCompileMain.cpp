int compileAccessResultHeader();
int compileConfigurationModelsHeader();
int compileDataAccessHeader();
int compileEnumTokensHeader();
int compileIntegrationPortsHeader();
int compileMeshModelsHeader();
int compilePostProcessRefsHeader();
int compileReferencesHeader();
int compileResourceModelsHeader();
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
        + compileIntegrationPortsHeader()
        + compileMeshModelsHeader()
        + compilePostProcessRefsHeader()
        + compileReferencesHeader()
        + compileResourceModelsHeader()
        + compileTaskModelsHeader()
        + compileTaskPortsHeader()
        + compileTelemetryHeader()
        + compileWorkflowModelsHeader();
}
