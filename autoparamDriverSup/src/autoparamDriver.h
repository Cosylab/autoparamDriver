#pragma once

#include "autoparamHandler.h"
#include <map>
#include <stdexcept>

namespace Autoparam {

class Driver : public asynPortDriver {
  protected:
    virtual Reason *createReason(Reason const &baseReason) {
        return new Reason(baseReason);
    }

  public:
    explicit Driver(const char *portName, int interfaceMask, int interruptMask,
                    int asynFlags, int autoConnect, int priority,
                    int stackSize);

    virtual ~Driver();

  protected:
    // reader ali writer je lahko NULL, če ga ni
    template <typename T>
    void registerHandlers(std::string const &function,
                          typename Handlers<T>::ReadHandler reader,
                          typename Handlers<T>::WriteHandler writer) {
        getHandlerMap<T>[function].readHandler = reader;
        getHandlerMap<T>[function].writeHandler = writer;
        m_functionTypes[function] = AsynType<T>::value;
    }

    // Priročni overloadi za uporabo io-interrupt recordov v derived driverju.
    // Tole je skupno vsem arrayem, zato šablona
    template <typename T>
    asynStatus doCallbacksArray(Reason const &reason, Array<T> &value,
                                int alarmStatus = epicsAlarmNone,
                                int alarmSeverity = epicsSevNone) {
        setParamAlarmStatus(reason.index(), alarmStatus);
        setParamAlarmSeverity(reason.index(), alarmSeverity);
        return doCallbacksArrayDispatch(reason.index(), value);
    }

    // Tole je pa wrapper v asyn stilu. Če hočemo tak stil, je treba pač
    // naštancat te.
    asynStatus doCallbacksInt32Array(Reason const &reason,
                                     Array<epicsInt32> &value,
                                     int alarmStatus = epicsAlarmNone,
                                     int alarmSeverity = epicsSevNone) {
        return doCallbacksArray(reason, value, alarmStatus, alarmSeverity);
    }
    // in tako dalje

    // Priročna funkcija za uporabo io-interrupt recordov v derived driverju. Ne
    // kličejo sama callParamCallbacks(), da lahko nastaviš več skalarjev,
    // preden ga pokličeš.
    template <typename T>
    asynStatus setParam(Reason const &reason, T value,
                        int alarmStatus = epicsAlarmNone,
                        int alarmSeverity = epicsSevNone) {
        setParamAlarmStatus(reason.index(), alarmStatus);
        setParamAlarmSeverity(reason.index(), alarmSeverity);
        return setParamDispatch(reason.index(), value);
    }

    // Tole je pa wrapper v asyn stilu. Ne morem se odločit, a bi npr. tole za
    // konsistentnost imenovali setInt32Param ali setIntegerParam :/
    asynStatus setInt32Param(Reason const &reason, epicsInt32 value,
                             int alarmStatus = epicsAlarmNone,
                             int alarmSeverity = epicsSevNone) {
        return setParam(reason, value, alarmStatus, alarmSeverity);
    }
    // in tako dalje

    // Od tule naprej so stvari sicer public zaradi interfacea, ampak se derived
    // driverja ne tičejo
  public:
    asynStatus drvUserCreate(asynUser *pasynUser, const char *reason,
                             const char **, size_t *);

    asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value) {
        return writeScalar(pasynUser, value);
    }
    // in tako dalje za vse write funkcije

    asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value) {
        return readScalar(pasynUser, value);
    }
    // in tako dalje za read funkcije

    asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *value,
                              size_t maxSize, size_t *size) {
        return readArray(pasynUser, value, maxSize, size);
    }

    // in tako dalje handlerji

  private:
    void handleResultStatus(asynUser *pasynUser, ResultBase const &result);

    Reason *reasonFromUser(asynUser *pasynUser);

    // Ni implementirano, specializeramo samo T-je, ki jih zna asyn
    template <typename T>
    asynStatus doCallbacksArrayDispatch(int index, Array<T> &value);

    // podobno naredimo za skalarne parametre.
    template <typename T> asynStatus setParamDispatch(int index, T value);

    template <typename T> asynStatus writeScalar(asynUser *pasynUser, T value);

    template <typename T> asynStatus readScalar(asynUser *pasynUser, T *value);

    template <typename T>
    asynStatus readArray(asynUser *pasynUser, T *value, size_t maxSize,
                         size_t *size);

    template <typename T> std::map<std::string, Handlers<T> > &getHandlerMap();

    static char const *driverName;

    typedef std::map<int, Reason *> ParamMap;
    ParamMap m_params;

    std::map<std::string, asynParamType> m_functionTypes;
    std::map<std::string, Handlers<epicsInt32> > m_int32HandlerMap;
    std::map<std::string, Handlers<epicsInt64> > m_float64HandlerMap;
    std::map<std::string, Handlers<Array<epicsInt16 > > >
        m_int16ArrayHandlerMap;
    // in tako dalje
};

char const *Driver::driverName = "Autoparam::Driver";

template <typename T>
asynStatus Driver::writeScalar(asynUser *pasynUser, T value) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<T>::WriteResult result;
    try {
        result = getHandlerMap<T>()
                     .at(reason->function())
                     .writeHandler(*reason, value);
    } catch (std::out_of_range const &) {
        // TODO
        // nimamo handlerja, parameter pa obstaja. Pisanje v takega ni kul,
        // zato pustimo, da result vsebuje soft alarm.
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        setParamDispatch(pasynUser->reason, value);
        callParamCallbacks();
    }
    return result.status;
}

template <typename T>
asynStatus Driver::readScalar(asynUser *pasynUser, T *value) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<T>::ReadResult result;
    try {
        result = getHandlerMap<T>().at(reason->function()).readHandler(*reason);
    } catch (std::out_of_range const &) {
        // nimamo handlerja, parameter pa obstaja. Branje takega ni kul,
        // zato pustimo, da result vsebuje soft alarm.
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *value = result.value;
    }
    return result.status;
}

template <typename T>
asynStatus Driver::readArray(asynUser *pasynUser, T *value, size_t maxSize,
                             size_t *size) {
    Reason *reason = reasonFromUser(pasynUser);
    if (!reason) {
        return asynError;
    }

    typename Handlers<Array<T> >::ReadResult result;
    try {
        result = getHandlerMap<Array<T> >()
                     .at(reason->function())
                     .readHandler(*reason);
    } catch (std::out_of_range const &) {
        // No handler
    }

    handleResultStatus(pasynUser, result);
    if (result.status == asynSuccess) {
        *size = std::min(result.value.size(), maxSize);
        std::copy(result.value.data(), result.value.data() + *size, value);
    }
    return result.status;
}

template <>
asynStatus Driver::setParamDispatch<epicsInt32>(int index, epicsInt32 value) {
    return setIntegerParam(index, value);
}
// in tako dalje specializacije za skalarje

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsInt32>(int index,
                                             Array<epicsInt32> &value) {
    return asynPortDriver::doCallbacksInt32Array(value.data(), value.size(),
                                                 index, 0);
}

template <>
asynStatus
Driver::doCallbacksArrayDispatch<epicsInt64>(int index,
                                             Array<epicsInt64> &value) {
    return asynPortDriver::doCallbacksInt64Array(value.data(), value.size(),
                                                 index, 0);
}
// in tako dalje specializacije za arraye

template <>
std::map<std::string, Handlers<epicsInt32> > &
Driver::getHandlerMap<epicsInt32>() {
    return m_int32HandlerMap;
}
// in tako dalje

} // namespace Autoparam
