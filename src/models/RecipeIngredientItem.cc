/**
 *
 *  RecipeIngredientItem.cc
 *  DO NOT EDIT. This file is generated by drogon_ctl
 *
 */

#include "RecipeIngredientItem.h"
#include <drogon/utils/Utilities.h>
#include <string>

using namespace drogon;
using namespace drogon::orm;
using namespace drogon_model::postgres;

const std::string RecipeIngredientItem::Cols::_recipe_id = "\"recipe_id\"";
const std::string RecipeIngredientItem::Cols::_item_id = "\"item_id\"";
const std::string RecipeIngredientItem::Cols::_slot = "\"slot\"";
const std::string RecipeIngredientItem::Cols::_count = "\"count\"";
const std::string RecipeIngredientItem::Cols::_input = "\"input\"";
const std::vector<std::string> RecipeIngredientItem::primaryKeyName = {"recipe_id","item_id","slot","input"};
const bool RecipeIngredientItem::hasPrimaryKey = true;
const std::string RecipeIngredientItem::tableName = "\"recipe_ingredient_item\"";

const std::vector<typename RecipeIngredientItem::MetaData> RecipeIngredientItem::metaData_={
{"recipe_id","int64_t","bigint",8,1,1,1},
{"item_id","std::string","character varying",255,0,1,1},
{"slot","int32_t","integer",4,0,1,1},
{"count","int32_t","integer",4,0,0,1},
{"input","bool","boolean",1,0,1,1}
};
const std::string &RecipeIngredientItem::getColumnName(size_t index) noexcept(false)
{
    assert(index < metaData_.size());
    return metaData_[index].colName_;
}
RecipeIngredientItem::RecipeIngredientItem(const Row &r, const ssize_t indexOffset) noexcept
{
    if(indexOffset < 0)
    {
        if(!r["recipe_id"].isNull())
        {
            recipeId_=std::make_shared<int64_t>(r["recipe_id"].as<int64_t>());
        }
        if(!r["item_id"].isNull())
        {
            itemId_=std::make_shared<std::string>(r["item_id"].as<std::string>());
        }
        if(!r["slot"].isNull())
        {
            slot_=std::make_shared<int32_t>(r["slot"].as<int32_t>());
        }
        if(!r["count"].isNull())
        {
            count_=std::make_shared<int32_t>(r["count"].as<int32_t>());
        }
        if(!r["input"].isNull())
        {
            input_=std::make_shared<bool>(r["input"].as<bool>());
        }
    }
    else
    {
        size_t offset = (size_t)indexOffset;
        if(offset + 5 > r.size())
        {
            LOG_FATAL << "Invalid SQL result for this model";
            return;
        }
        size_t index;
        index = offset + 0;
        if(!r[index].isNull())
        {
            recipeId_=std::make_shared<int64_t>(r[index].as<int64_t>());
        }
        index = offset + 1;
        if(!r[index].isNull())
        {
            itemId_=std::make_shared<std::string>(r[index].as<std::string>());
        }
        index = offset + 2;
        if(!r[index].isNull())
        {
            slot_=std::make_shared<int32_t>(r[index].as<int32_t>());
        }
        index = offset + 3;
        if(!r[index].isNull())
        {
            count_=std::make_shared<int32_t>(r[index].as<int32_t>());
        }
        index = offset + 4;
        if(!r[index].isNull())
        {
            input_=std::make_shared<bool>(r[index].as<bool>());
        }
    }

}

RecipeIngredientItem::RecipeIngredientItem(const Json::Value &pJson, const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if(pMasqueradingVector.size() != 5)
    {
        LOG_ERROR << "Bad masquerading vector";
        return;
    }
    if(!pMasqueradingVector[0].empty() && pJson.isMember(pMasqueradingVector[0]))
    {
        dirtyFlag_[0] = true;
        if(!pJson[pMasqueradingVector[0]].isNull())
        {
            recipeId_=std::make_shared<int64_t>((int64_t)pJson[pMasqueradingVector[0]].asInt64());
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
            slot_=std::make_shared<int32_t>((int32_t)pJson[pMasqueradingVector[2]].asInt64());
        }
    }
    if(!pMasqueradingVector[3].empty() && pJson.isMember(pMasqueradingVector[3]))
    {
        dirtyFlag_[3] = true;
        if(!pJson[pMasqueradingVector[3]].isNull())
        {
            count_=std::make_shared<int32_t>((int32_t)pJson[pMasqueradingVector[3]].asInt64());
        }
    }
    if(!pMasqueradingVector[4].empty() && pJson.isMember(pMasqueradingVector[4]))
    {
        dirtyFlag_[4] = true;
        if(!pJson[pMasqueradingVector[4]].isNull())
        {
            input_=std::make_shared<bool>(pJson[pMasqueradingVector[4]].asBool());
        }
    }
}

RecipeIngredientItem::RecipeIngredientItem(const Json::Value &pJson) noexcept(false)
{
    if(pJson.isMember("recipe_id"))
    {
        dirtyFlag_[0]=true;
        if(!pJson["recipe_id"].isNull())
        {
            recipeId_=std::make_shared<int64_t>((int64_t)pJson["recipe_id"].asInt64());
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
    if(pJson.isMember("slot"))
    {
        dirtyFlag_[2]=true;
        if(!pJson["slot"].isNull())
        {
            slot_=std::make_shared<int32_t>((int32_t)pJson["slot"].asInt64());
        }
    }
    if(pJson.isMember("count"))
    {
        dirtyFlag_[3]=true;
        if(!pJson["count"].isNull())
        {
            count_=std::make_shared<int32_t>((int32_t)pJson["count"].asInt64());
        }
    }
    if(pJson.isMember("input"))
    {
        dirtyFlag_[4]=true;
        if(!pJson["input"].isNull())
        {
            input_=std::make_shared<bool>(pJson["input"].asBool());
        }
    }
}

void RecipeIngredientItem::updateByMasqueradedJson(const Json::Value &pJson,
                                            const std::vector<std::string> &pMasqueradingVector) noexcept(false)
{
    if(pMasqueradingVector.size() != 5)
    {
        LOG_ERROR << "Bad masquerading vector";
        return;
    }
    if(!pMasqueradingVector[0].empty() && pJson.isMember(pMasqueradingVector[0]))
    {
        if(!pJson[pMasqueradingVector[0]].isNull())
        {
            recipeId_=std::make_shared<int64_t>((int64_t)pJson[pMasqueradingVector[0]].asInt64());
        }
    }
    if(!pMasqueradingVector[1].empty() && pJson.isMember(pMasqueradingVector[1]))
    {
        if(!pJson[pMasqueradingVector[1]].isNull())
        {
            itemId_=std::make_shared<std::string>(pJson[pMasqueradingVector[1]].asString());
        }
    }
    if(!pMasqueradingVector[2].empty() && pJson.isMember(pMasqueradingVector[2]))
    {
        if(!pJson[pMasqueradingVector[2]].isNull())
        {
            slot_=std::make_shared<int32_t>((int32_t)pJson[pMasqueradingVector[2]].asInt64());
        }
    }
    if(!pMasqueradingVector[3].empty() && pJson.isMember(pMasqueradingVector[3]))
    {
        dirtyFlag_[3] = true;
        if(!pJson[pMasqueradingVector[3]].isNull())
        {
            count_=std::make_shared<int32_t>((int32_t)pJson[pMasqueradingVector[3]].asInt64());
        }
    }
    if(!pMasqueradingVector[4].empty() && pJson.isMember(pMasqueradingVector[4]))
    {
        if(!pJson[pMasqueradingVector[4]].isNull())
        {
            input_=std::make_shared<bool>(pJson[pMasqueradingVector[4]].asBool());
        }
    }
}

void RecipeIngredientItem::updateByJson(const Json::Value &pJson) noexcept(false)
{
    if(pJson.isMember("recipe_id"))
    {
        if(!pJson["recipe_id"].isNull())
        {
            recipeId_=std::make_shared<int64_t>((int64_t)pJson["recipe_id"].asInt64());
        }
    }
    if(pJson.isMember("item_id"))
    {
        if(!pJson["item_id"].isNull())
        {
            itemId_=std::make_shared<std::string>(pJson["item_id"].asString());
        }
    }
    if(pJson.isMember("slot"))
    {
        if(!pJson["slot"].isNull())
        {
            slot_=std::make_shared<int32_t>((int32_t)pJson["slot"].asInt64());
        }
    }
    if(pJson.isMember("count"))
    {
        dirtyFlag_[3] = true;
        if(!pJson["count"].isNull())
        {
            count_=std::make_shared<int32_t>((int32_t)pJson["count"].asInt64());
        }
    }
    if(pJson.isMember("input"))
    {
        if(!pJson["input"].isNull())
        {
            input_=std::make_shared<bool>(pJson["input"].asBool());
        }
    }
}

const int64_t &RecipeIngredientItem::getValueOfRecipeId() const noexcept
{
    static const int64_t defaultValue = int64_t();
    if(recipeId_)
        return *recipeId_;
    return defaultValue;
}
const std::shared_ptr<int64_t> &RecipeIngredientItem::getRecipeId() const noexcept
{
    return recipeId_;
}
void RecipeIngredientItem::setRecipeId(const int64_t &pRecipeId) noexcept
{
    recipeId_ = std::make_shared<int64_t>(pRecipeId);
    dirtyFlag_[0] = true;
}

const std::string &RecipeIngredientItem::getValueOfItemId() const noexcept
{
    static const std::string defaultValue = std::string();
    if(itemId_)
        return *itemId_;
    return defaultValue;
}
const std::shared_ptr<std::string> &RecipeIngredientItem::getItemId() const noexcept
{
    return itemId_;
}
void RecipeIngredientItem::setItemId(const std::string &pItemId) noexcept
{
    itemId_ = std::make_shared<std::string>(pItemId);
    dirtyFlag_[1] = true;
}
void RecipeIngredientItem::setItemId(std::string &&pItemId) noexcept
{
    itemId_ = std::make_shared<std::string>(std::move(pItemId));
    dirtyFlag_[1] = true;
}

const int32_t &RecipeIngredientItem::getValueOfSlot() const noexcept
{
    static const int32_t defaultValue = int32_t();
    if(slot_)
        return *slot_;
    return defaultValue;
}
const std::shared_ptr<int32_t> &RecipeIngredientItem::getSlot() const noexcept
{
    return slot_;
}
void RecipeIngredientItem::setSlot(const int32_t &pSlot) noexcept
{
    slot_ = std::make_shared<int32_t>(pSlot);
    dirtyFlag_[2] = true;
}

const int32_t &RecipeIngredientItem::getValueOfCount() const noexcept
{
    static const int32_t defaultValue = int32_t();
    if(count_)
        return *count_;
    return defaultValue;
}
const std::shared_ptr<int32_t> &RecipeIngredientItem::getCount() const noexcept
{
    return count_;
}
void RecipeIngredientItem::setCount(const int32_t &pCount) noexcept
{
    count_ = std::make_shared<int32_t>(pCount);
    dirtyFlag_[3] = true;
}

const bool &RecipeIngredientItem::getValueOfInput() const noexcept
{
    static const bool defaultValue = bool();
    if(input_)
        return *input_;
    return defaultValue;
}
const std::shared_ptr<bool> &RecipeIngredientItem::getInput() const noexcept
{
    return input_;
}
void RecipeIngredientItem::setInput(const bool &pInput) noexcept
{
    input_ = std::make_shared<bool>(pInput);
    dirtyFlag_[4] = true;
}

void RecipeIngredientItem::updateId(const uint64_t id)
{
}
typename RecipeIngredientItem::PrimaryKeyType RecipeIngredientItem::getPrimaryKey() const
{
    return std::make_tuple(*recipeId_,*itemId_,*slot_,*input_);
}

const std::vector<std::string> &RecipeIngredientItem::insertColumns() noexcept
{
    static const std::vector<std::string> inCols={
        "item_id",
        "slot",
        "count",
        "input"
    };
    return inCols;
}

void RecipeIngredientItem::outputArgs(drogon::orm::internal::SqlBinder &binder) const
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
        if(getSlot())
        {
            binder << getValueOfSlot();
        }
        else
        {
            binder << nullptr;
        }
    }
    if(dirtyFlag_[3])
    {
        if(getCount())
        {
            binder << getValueOfCount();
        }
        else
        {
            binder << nullptr;
        }
    }
    if(dirtyFlag_[4])
    {
        if(getInput())
        {
            binder << getValueOfInput();
        }
        else
        {
            binder << nullptr;
        }
    }
}

const std::vector<std::string> RecipeIngredientItem::updateColumns() const
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
    if(dirtyFlag_[3])
    {
        ret.push_back(getColumnName(3));
    }
    if(dirtyFlag_[4])
    {
        ret.push_back(getColumnName(4));
    }
    return ret;
}

void RecipeIngredientItem::updateArgs(drogon::orm::internal::SqlBinder &binder) const
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
        if(getSlot())
        {
            binder << getValueOfSlot();
        }
        else
        {
            binder << nullptr;
        }
    }
    if(dirtyFlag_[3])
    {
        if(getCount())
        {
            binder << getValueOfCount();
        }
        else
        {
            binder << nullptr;
        }
    }
    if(dirtyFlag_[4])
    {
        if(getInput())
        {
            binder << getValueOfInput();
        }
        else
        {
            binder << nullptr;
        }
    }
}
Json::Value RecipeIngredientItem::toJson() const
{
    Json::Value ret;
    if(getRecipeId())
    {
        ret["recipe_id"]=(Json::Int64)getValueOfRecipeId();
    }
    else
    {
        ret["recipe_id"]=Json::Value();
    }
    if(getItemId())
    {
        ret["item_id"]=getValueOfItemId();
    }
    else
    {
        ret["item_id"]=Json::Value();
    }
    if(getSlot())
    {
        ret["slot"]=getValueOfSlot();
    }
    else
    {
        ret["slot"]=Json::Value();
    }
    if(getCount())
    {
        ret["count"]=getValueOfCount();
    }
    else
    {
        ret["count"]=Json::Value();
    }
    if(getInput())
    {
        ret["input"]=getValueOfInput();
    }
    else
    {
        ret["input"]=Json::Value();
    }
    return ret;
}

Json::Value RecipeIngredientItem::toMasqueradedJson(
    const std::vector<std::string> &pMasqueradingVector) const
{
    Json::Value ret;
    if(pMasqueradingVector.size() == 5)
    {
        if(!pMasqueradingVector[0].empty())
        {
            if(getRecipeId())
            {
                ret[pMasqueradingVector[0]]=(Json::Int64)getValueOfRecipeId();
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
            if(getSlot())
            {
                ret[pMasqueradingVector[2]]=getValueOfSlot();
            }
            else
            {
                ret[pMasqueradingVector[2]]=Json::Value();
            }
        }
        if(!pMasqueradingVector[3].empty())
        {
            if(getCount())
            {
                ret[pMasqueradingVector[3]]=getValueOfCount();
            }
            else
            {
                ret[pMasqueradingVector[3]]=Json::Value();
            }
        }
        if(!pMasqueradingVector[4].empty())
        {
            if(getInput())
            {
                ret[pMasqueradingVector[4]]=getValueOfInput();
            }
            else
            {
                ret[pMasqueradingVector[4]]=Json::Value();
            }
        }
        return ret;
    }
    LOG_ERROR << "Masquerade failed";
    if(getRecipeId())
    {
        ret["recipe_id"]=(Json::Int64)getValueOfRecipeId();
    }
    else
    {
        ret["recipe_id"]=Json::Value();
    }
    if(getItemId())
    {
        ret["item_id"]=getValueOfItemId();
    }
    else
    {
        ret["item_id"]=Json::Value();
    }
    if(getSlot())
    {
        ret["slot"]=getValueOfSlot();
    }
    else
    {
        ret["slot"]=Json::Value();
    }
    if(getCount())
    {
        ret["count"]=getValueOfCount();
    }
    else
    {
        ret["count"]=Json::Value();
    }
    if(getInput())
    {
        ret["input"]=getValueOfInput();
    }
    else
    {
        ret["input"]=Json::Value();
    }
    return ret;
}

bool RecipeIngredientItem::validateJsonForCreation(const Json::Value &pJson, std::string &err)
{
    if(pJson.isMember("recipe_id"))
    {
        if(!validJsonOfField(0, "recipe_id", pJson["recipe_id"], err, true))
            return false;
    }
    if(pJson.isMember("item_id"))
    {
        if(!validJsonOfField(1, "item_id", pJson["item_id"], err, true))
            return false;
    }
    else
    {
        err="The item_id column cannot be null";
        return false;
    }
    if(pJson.isMember("slot"))
    {
        if(!validJsonOfField(2, "slot", pJson["slot"], err, true))
            return false;
    }
    else
    {
        err="The slot column cannot be null";
        return false;
    }
    if(pJson.isMember("count"))
    {
        if(!validJsonOfField(3, "count", pJson["count"], err, true))
            return false;
    }
    else
    {
        err="The count column cannot be null";
        return false;
    }
    if(pJson.isMember("input"))
    {
        if(!validJsonOfField(4, "input", pJson["input"], err, true))
            return false;
    }
    else
    {
        err="The input column cannot be null";
        return false;
    }
    return true;
}
bool RecipeIngredientItem::validateMasqueradedJsonForCreation(const Json::Value &pJson,
                                                              const std::vector<std::string> &pMasqueradingVector,
                                                              std::string &err)
{
    if(pMasqueradingVector.size() != 5)
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
      if(!pMasqueradingVector[2].empty())
      {
          if(pJson.isMember(pMasqueradingVector[2]))
          {
              if(!validJsonOfField(2, pMasqueradingVector[2], pJson[pMasqueradingVector[2]], err, true))
                  return false;
          }
        else
        {
            err="The " + pMasqueradingVector[2] + " column cannot be null";
            return false;
        }
      }
      if(!pMasqueradingVector[3].empty())
      {
          if(pJson.isMember(pMasqueradingVector[3]))
          {
              if(!validJsonOfField(3, pMasqueradingVector[3], pJson[pMasqueradingVector[3]], err, true))
                  return false;
          }
        else
        {
            err="The " + pMasqueradingVector[3] + " column cannot be null";
            return false;
        }
      }
      if(!pMasqueradingVector[4].empty())
      {
          if(pJson.isMember(pMasqueradingVector[4]))
          {
              if(!validJsonOfField(4, pMasqueradingVector[4], pJson[pMasqueradingVector[4]], err, true))
                  return false;
          }
        else
        {
            err="The " + pMasqueradingVector[4] + " column cannot be null";
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
bool RecipeIngredientItem::validateJsonForUpdate(const Json::Value &pJson, std::string &err)
{
    if(pJson.isMember("recipe_id"))
    {
        if(!validJsonOfField(0, "recipe_id", pJson["recipe_id"], err, false))
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
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
    if(pJson.isMember("slot"))
    {
        if(!validJsonOfField(2, "slot", pJson["slot"], err, false))
            return false;
    }
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
    if(pJson.isMember("count"))
    {
        if(!validJsonOfField(3, "count", pJson["count"], err, false))
            return false;
    }
    if(pJson.isMember("input"))
    {
        if(!validJsonOfField(4, "input", pJson["input"], err, false))
            return false;
    }
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
    return true;
}
bool RecipeIngredientItem::validateMasqueradedJsonForUpdate(const Json::Value &pJson,
                                                            const std::vector<std::string> &pMasqueradingVector,
                                                            std::string &err)
{
    if(pMasqueradingVector.size() != 5)
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
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
      if(!pMasqueradingVector[2].empty() && pJson.isMember(pMasqueradingVector[2]))
      {
          if(!validJsonOfField(2, pMasqueradingVector[2], pJson[pMasqueradingVector[2]], err, false))
              return false;
      }
    else
    {
        err = "The value of primary key must be set in the json object for update";
        return false;
    }
      if(!pMasqueradingVector[3].empty() && pJson.isMember(pMasqueradingVector[3]))
      {
          if(!validJsonOfField(3, pMasqueradingVector[3], pJson[pMasqueradingVector[3]], err, false))
              return false;
      }
      if(!pMasqueradingVector[4].empty() && pJson.isMember(pMasqueradingVector[4]))
      {
          if(!validJsonOfField(4, pMasqueradingVector[4], pJson[pMasqueradingVector[4]], err, false))
              return false;
      }
    else
    {
        err = "The value of primary key must be set in the json object for update";
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
bool RecipeIngredientItem::validJsonOfField(size_t index,
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
        case 2:
            if(pJson.isNull())
            {
                err="The " + fieldName + " column cannot be null";
                return false;
            }
            if(!pJson.isInt())
            {
                err="Type error in the "+fieldName+" field";
                return false;
            }
            break;
        case 3:
            if(pJson.isNull())
            {
                err="The " + fieldName + " column cannot be null";
                return false;
            }
            if(!pJson.isInt())
            {
                err="Type error in the "+fieldName+" field";
                return false;
            }
            break;
        case 4:
            if(pJson.isNull())
            {
                err="The " + fieldName + " column cannot be null";
                return false;
            }
            if(!pJson.isBool())
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
