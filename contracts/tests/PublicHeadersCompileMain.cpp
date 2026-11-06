int compileDataAccessHeader();
int compileIntegrationPortsHeader();
int compilePostProcessRefsHeader();
int compileReferencesHeader();
int compileTaskModelsHeader();
int compileTaskPortsHeader();
int compileTelemetryHeader();
int compileWorkflowModelsHeader();

int main()
{
    return compileDataAccessHeader()
        + compileIntegrationPortsHeader()
        + compilePostProcessRefsHeader()
        + compileReferencesHeader()
        + compileTaskModelsHeader()
        + compileTaskPortsHeader()
        + compileTelemetryHeader()
        + compileWorkflowModelsHeader();
}
