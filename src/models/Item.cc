/**
 *
 *  Item.cc
 *  DO NOT EDIT. This file is generated by drogon_ctl
 *
 */

#include "Item.h"
#include <drogon/utils/Utilities.h>
#include <string>

using namespace drogon;
using namespace drogon::orm;
using namespace drogon_model::postgres;

const std::string Item::Cols::_id = "\"id\"";
const std::string Item::Cols::_loc = "\"loc\"";
const std::string Item::primaryKeyName = "id";
const bool Item::hasPrimaryKey = true;
const std::string Item::tableName = "\"item\"";

const std::vector<typename Item::MetaData> Item::metaData_={
{"id","int64_t","bigint",8,1,1,1},
{"loc","std::string","character varying",255,0,0,1}
};
const std::string &Item::getColumnName(size_t index) noexcept(false)
{
    assert(index < metaData_.size());
    return metaData_[index].colName_;
}
Item::Item(const Row &r, const ssize_t indexOffset) noexcept
{
    if(indexOffset < 0)
    {
        if(!r["id"].isNull())
        {
            id_=std::make_shared<int64_t>(r["id"].as<int64_t>());
        }
        if(!r["loc"].isNull())
        {
            loc_=std::make_shared<std::string>(r["loc"].as<std::string>());
        }
    }
    else
    {
        size_t offset = (size_t)indexOffset;
        if(offset + 2 > r.size())
        {
            LOG_FATAL << "Invalid SQL result for this model";
            return;
        }
        size_t index;
        index = offset + 0;
        if(!r[index].isNull())
        {
            id_=std::make_shared<int64_t>(r[index].as<int64_t>());
        }
        index = offset + 1;
        if(!r[index].isNull())
        {
            loc_=std::make_shared<std::string>(r[index].as<std::string>());
        }
    }

}

Item::Item(const Json::Value &pJson, const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if(pMasqueradingVector.size() != 2)
    {
        LOG_ERROR << "Bad masquerading vector";
        return;
    }
    if(!pMasqueradingVector[0].empty() && pJson.isMember(pMasqueradingVector[0]))
    {
        dirtyFlag_[0] = true;
        if(!pJson[pMasqueradingVector[0]].isNull())
        {
            id_=std::make_shared<int64_t>((int64_t)pJson[pMasqueradingVector[0]].asInt64());
        }
    }
    if(!pMasqueradingVector[1].empty() && pJson.isMember(pMasqueradingVector[1]))
    {
        dirtyFlag_[1] = true;
        if(!pJson[pMasqueradingVector[1]].isNull())
        {
            loc_=std::make_shared<std::string>(pJson[pMasqueradingVector[1]].asString());
        }
    }
}

Item::Item(const Json::Value &pJson) noexcept(false)
{
    if(pJson.isMember("id"))
    {
        dirtyFlag_[0]=true;
        if(!pJson["id"].isNull())
        {
            id_=std::make_shared<int64_t>((int64_t)pJson["id"].asInt64());
        }
    }
    if(pJson.isMember("loc"))
    {
        dirtyFlag_[1]=true;
        if(!pJson["loc"].isNull())
        {
            loc_=std::make_shared<std::string>(pJson["loc"].asString());
        }
    }
}

void Item::updateByMasqueradedJson(const Json::Value &pJson,
                                            const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if(pMasqueradingVector.size() != 2)
    {
        LOG_ERROR << "Bad masquerading vector";
        return;
    }
    if(!pMasqueradingVector[0].empty() && pJson.isMember(pMasqueradingVector[0]))
    {
        if(!pJson[pMasqueradingVector[0]].isNull())
        {
            id_=std::make_shared<int64_t>((int64_t)pJson[pMasqueradingVector[0]].asInt64());
        }
    }
    if(!pMasqueradingVector[1].empty() && pJson.isMember(pMasqueradingVector[1]))
    {
        dirtyFlag_[1] = true;
        if(!pJson[pMasqueradingVector[1]].isNull())
        {
            loc_=std::make_shared<std::string>(pJson[pMasqueradingVector[1]].asString());
        }
    }
}

void Item::updateByJson(const Json::Value &pJson) noexcept(false)
{
    if(pJson.isMember("id"))
    {
        if(!pJson["id"].isNull())
        {
            id_=std::make_shared<int64_t>((int64_t)pJson["id"].asInt64());
        }
    }
    if(pJson.isMember("loc"))
    {
        dirtyFlag_[1] = true;
        if(!pJson["loc"].isNull())
        {
            loc_=std::make_shared<std::string>(pJson["loc"].asString());
        }
    }
}

const int64_t &Item::getValueOfId() const noexcept
{
    static const int64_t defaultValue = int64_t();
    if(id_)
        return *id_;
    return defaultValue;
}
const std::shared_ptr<int64_t> &Item::getId() const noexcept
{
    return id_;
}
void Item::setId(const int64_t &pId) noexcept
{
    id_ = std::make_shared<int64_t>(pId);
    dirtyFlag_[0] = true;
}
const typename Item::PrimaryKeyType & Item::getPrimaryKey() const
{
    assert(id_);
    return *id_;
}

const std::string &Item::getValueOfLoc() const noexcept
{
    static const std::string defaultValue = std::string();
    if(loc_)
        return *loc_;
    return defaultValue;
}
const std::shared_ptr<std::string> &Item::getLoc() const noexcept
{
    return loc_;
}
void Item::setLoc(const std::string &pLoc) noexcept
{
    loc_ = std::make_shared<std::string>(pLoc);
    dirtyFlag_[1] = true;
}
void Item::setLoc(std::string &&pLoc) noexcept
{
    loc_ = std::make_shared<std::string>(std::move(pLoc));
    dirtyFlag_[1] = true;
}

void Item::updateId(const uint64_t id)
{
}

const std::vector<std::string> &Item::insertColumns() noexcept
{
    static const std::vector<std::string> inCols={
        "loc"
    };
    return inCols;
}

void Item::outputArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if(dirtyFlag_[1])
    {
        if(getLoc())
        {
            binder << getValueOfLoc();
        }
        else
        {
            binder << nullptr;
        }
    }
}

const std::vector<std::string> Item::updateColumns() const
{
    std::vector<std::string> ret;
    if(dirtyFlag_[1])
    {
        ret.push_back(getColumnName(1));
    }
    return ret;
}

void Item::updateArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if(dirtyFlag_[1])
    {
        if(getLoc())
        {
            binder << getValueOfLoc();
        }
        else
        {
            binder << nullptr;
        }
    }
}
Json::Value Item::toJson() const
{
    Json::Value ret;
    if(getId())
    {
        ret["id"]=(Json::Int64)getValueOfId();
    }
    else
    {
        ret["id"]=Json::Value();
    }
    if(getLoc())
    {
        ret["loc"]=getValueOfLoc();
    }
    else
    {
        ret["loc"]=Json::Value();
    }
    return ret;
}

Json::Value Item::toMasqueradedJson(
    const std::vector<std::string> &pMasqueradingVector) const
{
    Json::Value ret;
    if(pMasqueradingVector.size() == 2)
    {
        if(!pMasqueradingVector[0].empty())
        {
            if(getId())
            {
                ret[pMasqueradingVector[0]]=(Json::Int64)getValueOfId();
            }
            else
            {
                ret[pMasqueradingVector[0]]=Json::Value();
            }
        }
        if(!pMasqueradingVector[1].empty())
        {
            if(getLoc())
            {
                ret[pMasqueradingVector[1]]=getValueOfLoc();
            }
            else
            {
                ret[pMasqueradingVector[1]]=Json::Value();
            }
        }
        return ret;
    }
    LOG_ERROR << "Masquerade failed";
    if(getId())
    {
        ret["id"]=(Json::Int64)getValueOfId();
    }
    else
    {
        ret["id"]=Json::Value();
    }
    if(getLoc())
    {
        ret["loc"]=getValueOfLoc();
    }
    else
    {
        ret["loc"]=Json::Value();
    }
    return ret;
}

bool Item::validateJsonForCreation(const Json::Value &pJson, std::string &err)
{
    if(pJson.isMember("id"))
    {
        if(!validJsonOfField(0, "id", pJson["id"], err, true))
            return false;
    }
    if(pJson.isMember("loc"))
    {
        if(!validJsonOfField(1, "loc", pJson["loc"], err, true))
            return false;
    }
    else
    {
        err="The loc column cannot be null";
        return false;
    }
    return true;
}
bool Item::validateMasqueradedJsonForCreation(const Json::Value &pJson,
                                              const std::vector<std::string> &pMasqueradingVector,
                                              std::string &err)
{
    if(pMasqueradingVector.size() != 2)
    {
        err = "Bad masquerading vector";
        return false;
    }
    try {
      if(!pMasqueradingVector[0].empty())
      {
          if(pJson.isMember(pMasqueradingVector[0]))
          {
              if(!validJsonOfField(0, pMasqueradingVector[0], pJson[pMasqueradingVector[0]], err, true))
                  return false;
          }
      }
      if(!pMasqueradingVector[1].empty())
      {
          if(pJson.isMember(pMasqueradingVector[1]))
          {
              if(!validJsonOfField(1, pMasqueradingVector[1], pJson[pMasqueradingVector[1]], err, true))
                  return false;
          }
        else
        {
            err="The " + pMasqueradingVector[1] + " column cannot be null";
            return false;
        }
      }
    }
    catch(const Json::LogicError &e)
    {
      err = e.what();
      return false;
    }
    return true;
}
bool Item::validateJsonForUpdate(const Json::Value &pJson, std::string &err)
{
    if(pJson.isMember("id"))
    {
        if(!validJsonOfField(0, "id", pJson["id"], err, false))
            return false;
    }
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
    if(pJson.isMember("loc"))
    {
        if(!validJsonOfField(1, "loc", pJson["loc"], err, false))
            return false;
    }
    return true;
}
bool Item::validateMasqueradedJsonForUpdate(const Json::Value &pJson,
                                            const std::vector<std::string> &pMasqueradingVector,
                                            std::string &err)
{
    if(pMasqueradingVector.size() != 2)
    {
        err = "Bad masquerading vector";
        return false;
    }
    try {
      if(!pMasqueradingVector[0].empty() && pJson.isMember(pMasqueradingVector[0]))
      {
          if(!validJsonOfField(0, pMasqueradingVector[0], pJson[pMasqueradingVector[0]], err, false))
              return false;
      }
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
      if(!pMasqueradingVector[1].empty() && pJson.isMember(pMasqueradingVector[1]))
      {
          if(!validJsonOfField(1, pMasqueradingVector[1], pJson[pMasqueradingVector[1]], err, false))
              return false;
      }
    }
    catch(const Json::LogicError &e)
    {
      err = e.what();
      return false;
    }
    return true;
}
bool Item::validJsonOfField(size_t index,
                            const std::string &fieldName,
                            const Json::Value &pJson,
                            std::string &err,
                            bool isForCreation)
{
    switch(index)
    {
        case 0:
            if(pJson.isNull())
            {
                err="The " + fieldName + " column cannot be null";
                return false;
            }
            if(isForCreation)
            {
                err="The automatic primary key cannot be set";
                return false;
            }
            if(!pJson.isInt64())
            {
                err="Type error in the "+fieldName+" field";
                return false;
            }
            break;
        case 1:
            if(pJson.isNull())
            {
                err="The " + fieldName + " column cannot be null";
                return false;
            }
            if(!pJson.isString())
            {
                err="Type error in the "+fieldName+" field";
                return false;
            }
            if(pJson.isString() && std::strlen(pJson.asCString()) > 255)
            {
                err="String length exceeds limit for the " +
                    fieldName +
                    " field (the maximum value is 255)";
                return false;
            }

            break;
        default:
            err="Internal error in the server";
            return false;
    }
    return true;
}
