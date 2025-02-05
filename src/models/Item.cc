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
const std::string Item::Cols::_item_id = "\"item_id\"";
const std::string Item::Cols::_project_id = "\"project_id\"";
const std::string Item::primaryKeyName = "id";
const bool Item::hasPrimaryKey = true;
const std::string Item::tableName = "\"item\"";

const std::vector<typename Item::MetaData> Item::metaData_={
{"id","int64_t","bigint",8,1,1,1},
{"item_id","std::string","character varying",255,0,0,0},
{"project_id","std::string","text",0,0,0,0}
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
        if(!r["item_id"].isNull())
        {
            itemId_=std::make_shared<std::string>(r["item_id"].as<std::string>());
        }
        if(!r["project_id"].isNull())
        {
            projectId_=std::make_shared<std::string>(r["project_id"].as<std::string>());
        }
    }
    else
    {
        size_t offset = (size_t)indexOffset;
        if(offset + 3 > r.size())
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
            itemId_=std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 2;
        if(!r[index].isNull())
        {
            projectId_=std::make_shared<std::string>(r[index].as<std::string>());
        }
    }

}

Item::Item(const Json::Value &pJson, const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if(pMasqueradingVector.size() != 3)
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
            itemId_=std::make_shared<std::string>(pJson[pMasqueradingVector[1]].asString());
        }
    }
    if(!pMasqueradingVector[2].empty() && pJson.isMember(pMasqueradingVector[2]))
    {
        dirtyFlag_[2] = true;
        if(!pJson[pMasqueradingVector[2]].isNull())
        {
            projectId_=std::make_shared<std::string>(pJson[pMasqueradingVector[2]].asString());
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
    if(pJson.isMember("item_id"))
    {
        dirtyFlag_[1]=true;
        if(!pJson["item_id"].isNull())
        {
            itemId_=std::make_shared<std::string>(pJson["item_id"].asString());
        }
    }
    if(pJson.isMember("project_id"))
    {
        dirtyFlag_[2]=true;
        if(!pJson["project_id"].isNull())
        {
            projectId_=std::make_shared<std::string>(pJson["project_id"].asString());
        }
    }
}

void Item::updateByMasqueradedJson(const Json::Value &pJson,
                                            const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if(pMasqueradingVector.size() != 3)
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
            itemId_=std::make_shared<std::string>(pJson[pMasqueradingVector[1]].asString());
        }
    }
    if(!pMasqueradingVector[2].empty() && pJson.isMember(pMasqueradingVector[2]))
    {
        dirtyFlag_[2] = true;
        if(!pJson[pMasqueradingVector[2]].isNull())
        {
            projectId_=std::make_shared<std::string>(pJson[pMasqueradingVector[2]].asString());
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
    if(pJson.isMember("item_id"))
    {
        dirtyFlag_[1] = true;
        if(!pJson["item_id"].isNull())
        {
            itemId_=std::make_shared<std::string>(pJson["item_id"].asString());
        }
    }
    if(pJson.isMember("project_id"))
    {
        dirtyFlag_[2] = true;
        if(!pJson["project_id"].isNull())
        {
            projectId_=std::make_shared<std::string>(pJson["project_id"].asString());
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

const std::string &Item::getValueOfItemId() const noexcept
{
    static const std::string defaultValue = std::string();
    if(itemId_)
        return *itemId_;
    return defaultValue;
}
const std::shared_ptr<std::string> &Item::getItemId() const noexcept
{
    return itemId_;
}
void Item::setItemId(const std::string &pItemId) noexcept
{
    itemId_ = std::make_shared<std::string>(pItemId);
    dirtyFlag_[1] = true;
}
void Item::setItemId(std::string &&pItemId) noexcept
{
    itemId_ = std::make_shared<std::string>(std::move(pItemId));
    dirtyFlag_[1] = true;
}
void Item::setItemIdToNull() noexcept
{
    itemId_.reset();
    dirtyFlag_[1] = true;
}

const std::string &Item::getValueOfProjectId() const noexcept
{
    static const std::string defaultValue = std::string();
    if(projectId_)
        return *projectId_;
    return defaultValue;
}
const std::shared_ptr<std::string> &Item::getProjectId() const noexcept
{
    return projectId_;
}
void Item::setProjectId(const std::string &pProjectId) noexcept
{
    projectId_ = std::make_shared<std::string>(pProjectId);
    dirtyFlag_[2] = true;
}
void Item::setProjectId(std::string &&pProjectId) noexcept
{
    projectId_ = std::make_shared<std::string>(std::move(pProjectId));
    dirtyFlag_[2] = true;
}
void Item::setProjectIdToNull() noexcept
{
    projectId_.reset();
    dirtyFlag_[2] = true;
}

void Item::updateId(const uint64_t id)
{
}

const std::vector<std::string> &Item::insertColumns() noexcept
{
    static const std::vector<std::string> inCols={
        "item_id",
        "project_id"
    };
    return inCols;
}

void Item::outputArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if(dirtyFlag_[1])
    {
        if(getItemId())
        {
            binder << getValueOfItemId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if(dirtyFlag_[2])
    {
        if(getProjectId())
        {
            binder << getValueOfProjectId();
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
    if(dirtyFlag_[2])
    {
        ret.push_back(getColumnName(2));
    }
    return ret;
}

void Item::updateArgs(drogon::orm::internal::SqlBinder &binder) const
{
    if(dirtyFlag_[1])
    {
        if(getItemId())
        {
            binder << getValueOfItemId();
        }
        else
        {
            binder << nullptr;
        }
    }
    if(dirtyFlag_[2])
    {
        if(getProjectId())
        {
            binder << getValueOfProjectId();
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
    if(getItemId())
    {
        ret["item_id"]=getValueOfItemId();
    }
    else
    {
        ret["item_id"]=Json::Value();
    }
    if(getProjectId())
    {
        ret["project_id"]=getValueOfProjectId();
    }
    else
    {
        ret["project_id"]=Json::Value();
    }
    return ret;
}

Json::Value Item::toMasqueradedJson(
    const std::vector<std::string> &pMasqueradingVector) const
{
    Json::Value ret;
    if(pMasqueradingVector.size() == 3)
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
            if(getItemId())
            {
                ret[pMasqueradingVector[1]]=getValueOfItemId();
            }
            else
            {
                ret[pMasqueradingVector[1]]=Json::Value();
            }
        }
        if(!pMasqueradingVector[2].empty())
        {
            if(getProjectId())
            {
                ret[pMasqueradingVector[2]]=getValueOfProjectId();
            }
            else
            {
                ret[pMasqueradingVector[2]]=Json::Value();
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
    if(getItemId())
    {
        ret["item_id"]=getValueOfItemId();
    }
    else
    {
        ret["item_id"]=Json::Value();
    }
    if(getProjectId())
    {
        ret["project_id"]=getValueOfProjectId();
    }
    else
    {
        ret["project_id"]=Json::Value();
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
    if(pJson.isMember("item_id"))
    {
        if(!validJsonOfField(1, "item_id", pJson["item_id"], err, true))
            return false;
    }
    if(pJson.isMember("project_id"))
    {
        if(!validJsonOfField(2, "project_id", pJson["project_id"], err, true))
            return false;
    }
    return true;
}
bool Item::validateMasqueradedJsonForCreation(const Json::Value &pJson,
                                              const std::vector<std::string> &pMasqueradingVector,
                                              std::string &err)
{
    if(pMasqueradingVector.size() != 3)
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
      }
      if(!pMasqueradingVector[2].empty())
      {
          if(pJson.isMember(pMasqueradingVector[2]))
          {
              if(!validJsonOfField(2, pMasqueradingVector[2], pJson[pMasqueradingVector[2]], err, true))
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
    if(pJson.isMember("item_id"))
    {
        if(!validJsonOfField(1, "item_id", pJson["item_id"], err, false))
            return false;
    }
    if(pJson.isMember("project_id"))
    {
        if(!validJsonOfField(2, "project_id", pJson["project_id"], err, false))
            return false;
    }
    return true;
}
bool Item::validateMasqueradedJsonForUpdate(const Json::Value &pJson,
                                            const std::vector<std::string> &pMasqueradingVector,
                                            std::string &err)
{
    if(pMasqueradingVector.size() != 3)
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
      if(!pMasqueradingVector[2].empty() && pJson.isMember(pMasqueradingVector[2]))
      {
          if(!validJsonOfField(2, pMasqueradingVector[2], pJson[pMasqueradingVector[2]], err, false))
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
                return true;
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
        case 2:
            if(pJson.isNull())
            {
                return true;
            }
            if(!pJson.isString())
            {
                err="Type error in the "+fieldName+" field";
                return false;
            }
            break;
        default:
            err="Internal error in the server";
            return false;
    }
    return true;
}
