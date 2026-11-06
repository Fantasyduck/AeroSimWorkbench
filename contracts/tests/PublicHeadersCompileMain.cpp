int compileAccessResultHeader();
int compileDataAccessHeader();
int compileEnumTokensHeader();
int compileIntegrationPortsHeader();
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
        + compileDataAccessHeader()
        + compileEnumTokensHeader()
        + compileIntegrationPortsHeader()
        + compilePostProcessRefsHeader()
        + compileReferencesHeader()
        + compileResourceModelsHeader()
        + compileTaskModelsHeader()
        + compileTaskPortsHeader()
        + compileTelemetryHeader()
        + compileWorkflowModelsHeader();
}
