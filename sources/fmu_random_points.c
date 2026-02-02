#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fmi2Functions.h"

#define NUM_POINTS 500

typedef struct {
    fmi2CallbackFunctions callbacks;
    fmi2Real time;
    fmi2Real startTime;
    fmi2Real y;
    fmi2Integer idx;
    fmi2Integer seed;
    fmi2Real samplePeriod;
    fmi2Real points[NUM_POINTS];
    fmi2Boolean initialized;
    fmi2Boolean inInitialization;
} ModelInstance;

static unsigned int lcg_next(unsigned int* state) {
    *state = (*state) * 1664525u + 1013904223u;  
    return *state;
}

static void generate_points(ModelInstance* inst) {
    unsigned int state = (unsigned int)inst->seed;
    for (int i = 0; i < NUM_POINTS; ++i) {
        unsigned int r = lcg_next(&state);
        double u = (double)(r & 0x00FFFFFFu) / 16777215.0; /* [0,1] */
        inst->points[i] = 0.0 + u * 100.0;
    }
}

static ModelInstance* to_instance(fmi2Component c) {
    return (ModelInstance*)c;
}

FMI2_Export fmi2Status fmi2SetDebugLogging(
    fmi2Component c,
    fmi2Boolean loggingOn,
    size_t nCategories,
    const fmi2String categories[]) {

    (void)c;
    (void)loggingOn;
    (void)nCategories;
    (void)categories;
    return fmi2OK;
}

FMI2_Export fmi2String fmi2GetVersion(void) {
    return fmi2Version;
}

FMI2_Export fmi2String fmi2GetTypesPlatform(void) {
    return fmi2TypesPlatform;
}

FMI2_Export fmi2Component fmi2Instantiate(
    fmi2String instanceName,
    fmi2Type fmuType,
    fmi2String fmuGUID,
    fmi2String fmuResourceLocation,
    const fmi2CallbackFunctions* functions,
    fmi2Boolean visible,
    fmi2Boolean loggingOn) {

    (void)instanceName;
    (void)fmuType;
    (void)fmuGUID;
    (void)fmuResourceLocation;
    (void)visible;
    (void)loggingOn;

    if (!functions || !functions->allocateMemory || !functions->freeMemory) {
        return NULL;
    }

    ModelInstance* inst = (ModelInstance*)functions->allocateMemory(1, sizeof(ModelInstance));
    if (!inst) {
        return NULL;
    }

    memset(inst, 0, sizeof(ModelInstance));
    inst->callbacks = *functions;
    inst->time = 0.0;
    inst->startTime = 0.0;
    inst->y = 0.0;
    inst->idx = 0;
    inst->seed = 1;
    inst->samplePeriod = 1.0;
    inst->initialized = fmi2False;
    inst->inInitialization = fmi2False;

    return (fmi2Component)inst;
}

FMI2_Export fmi2Status fmi2SetupExperiment(
    fmi2Component c,
    fmi2Boolean toleranceDefined,
    fmi2Real tolerance,
    fmi2Real startTime,
    fmi2Boolean stopTimeDefined,
    fmi2Real stopTime) {

    (void)toleranceDefined;
    (void)tolerance;
    (void)stopTimeDefined;
    (void)stopTime;

    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }

    inst->startTime = startTime;
    inst->time = startTime;
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2EnterInitializationMode(fmi2Component c) {
    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }
    inst->inInitialization = fmi2True;
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2ExitInitializationMode(fmi2Component c) {
    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }
    if (inst->samplePeriod <= 0.0) {
        return fmi2Error;
    }
    generate_points(inst);
    inst->initialized = fmi2True;
    inst->inInitialization = fmi2False;
    inst->idx = 0;
    inst->y = inst->points[0];
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2DoStep(
    fmi2Component c,
    fmi2Real currentCommunicationPoint,
    fmi2Real communicationStepSize,
    fmi2Boolean noSetFMUStatePriorToCurrentPoint) {

    (void)noSetFMUStatePriorToCurrentPoint;

    ModelInstance* inst = to_instance(c);
    if (!inst || !inst->initialized) {
        return fmi2Error;
    }

    inst->time = currentCommunicationPoint + communicationStepSize;

    fmi2Real tRel = inst->time - inst->startTime;
    int idx = (int)floor(tRel / inst->samplePeriod + 1e-12);
    if (idx < 0) idx = 0;
    if (idx >= NUM_POINTS) idx = NUM_POINTS - 1;

    inst->idx = (fmi2Integer)idx;
    inst->y = inst->points[idx];

    return fmi2OK;
}

FMI2_Export fmi2Status fmi2Terminate(fmi2Component c) {
    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }
    return fmi2OK;
}

FMI2_Export void fmi2FreeInstance(fmi2Component c) {
    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return;
    }
    inst->callbacks.freeMemory(inst);
}

FMI2_Export fmi2Status fmi2Reset(fmi2Component c) {
    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }
    inst->time = inst->startTime;
    inst->y = 0.0;
    inst->idx = 0;
    inst->initialized = fmi2False;
    inst->inInitialization = fmi2False;
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2GetReal(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Real value[]) {

    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }

    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case 0: value[i] = inst->y; break;
            case 3: value[i] = inst->samplePeriod; break;
            default: return fmi2Error;
        }
    }
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2SetReal(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Real value[]) {

    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }

    if (!inst->inInitialization && inst->initialized) {
        return fmi2OK;
    }

    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case 3: inst->samplePeriod = value[i]; break;
            default: return fmi2Error;
        }
    }
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2GetInteger(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Integer value[]) {

    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }

    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case 1: value[i] = inst->idx; break;
            case 2: value[i] = inst->seed; break;
            default: return fmi2Error;
        }
    }
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2SetInteger(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Integer value[]) {

    ModelInstance* inst = to_instance(c);
    if (!inst) {
        return fmi2Error;
    }

    if (!inst->inInitialization && inst->initialized) {
        return fmi2OK;
    }

    for (size_t i = 0; i < nvr; ++i) {
        switch (vr[i]) {
            case 2: inst->seed = value[i]; break;
            default: return fmi2Error;
        }
    }
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2SetRealInputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr,
    const fmi2Integer order[], const fmi2Real value[]) {

    (void)c; (void)vr; (void)nvr; (void)order; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetRealOutputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr,
    const fmi2Integer order[], fmi2Real value[]) {

    (void)c; (void)vr; (void)nvr; (void)order; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetRealInputDerivatives(
    fmi2Component c,
    const fmi2ValueReference vr[], size_t nvr,
    const fmi2Integer order[], fmi2Real value[]) {

    (void)c; (void)vr; (void)nvr; (void)order; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2CancelStep(fmi2Component c) {
    (void)c;
    return fmi2OK;
}

FMI2_Export fmi2Status fmi2GetStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Status* value) {
    (void)c; (void)s; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetRealStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Real* value) {
    (void)c; (void)s; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetIntegerStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Integer* value) {
    (void)c; (void)s; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetBooleanStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2Boolean* value) {
    (void)c; (void)s; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetStringStatus(
    fmi2Component c, const fmi2StatusKind s, fmi2String* value) {
    (void)c; (void)s; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetBoolean(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2Boolean value[]) {
    (void)c; (void)vr; (void)nvr; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetString(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, const fmi2String value[]) {
    (void)c; (void)vr; (void)nvr; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetBoolean(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2Boolean value[]) {
    (void)c; (void)vr; (void)nvr; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetString(
    fmi2Component c, const fmi2ValueReference vr[], size_t nvr, fmi2String value[]) {
    (void)c; (void)vr; (void)nvr; (void)value;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetFMUstate(
    fmi2Component c, fmi2FMUstate* FMUstate) {
    (void)c; (void)FMUstate;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SetFMUstate(
    fmi2Component c, fmi2FMUstate FMUstate) {
    (void)c; (void)FMUstate;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2FreeFMUstate(
    fmi2Component c, fmi2FMUstate* FMUstate) {
    (void)c; (void)FMUstate;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SerializedFMUstateSize(
    fmi2Component c, fmi2FMUstate FMUstate, size_t* size) {
    (void)c; (void)FMUstate; (void)size;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2SerializeFMUstate(
    fmi2Component c, fmi2FMUstate FMUstate, fmi2Byte serializedState[], size_t size) {
    (void)c; (void)FMUstate; (void)serializedState; (void)size;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2DeSerializeFMUstate(
    fmi2Component c, const fmi2Byte serializedState[], size_t size, fmi2FMUstate* FMUstate) {
    (void)c; (void)serializedState; (void)size; (void)FMUstate;
    return fmi2Error;
}

FMI2_Export fmi2Status fmi2GetDirectionalDerivative(
    fmi2Component c,
    const fmi2ValueReference vUnknown_ref[], size_t nUnknown,
    const fmi2ValueReference vKnown_ref[], size_t nKnown,
    const fmi2Real dvKnown[], fmi2Real dvUnknown[]) {

    (void)c; (void)vUnknown_ref; (void)nUnknown; (void)vKnown_ref; (void)nKnown; (void)dvKnown; (void)dvUnknown;
    return fmi2Error;
}
