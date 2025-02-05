/**
 *
 *  RecipeIngredientTag.h
 *  DO NOT EDIT. This file is generated by drogon_ctl
 *
 */

#pragma once
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/orm/Field.h>
#include <drogon/orm/SqlBinder.h>
#include <drogon/orm/Mapper.h>
#include <drogon/orm/BaseBuilder.h>
#ifdef __cpp_impl_coroutine
#include <drogon/orm/CoroMapper.h>
#endif
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <tuple>
#include <stdint.h>
#include <iostream>

namespace drogon
{
namespace orm
{
class DbClient;
using DbClientPtr = std::shared_ptr<DbClient>;
}
}
namespace drogon_model
{
namespace postgres
{

class RecipeIngredientTag
{
  public:
    struct Cols
    {
        static const std::string _recipe_id;
        static const std::string _tag_id;
        static const std::string _slot;
        static const std::string _count;
        static const std::string _input;
    };

    static const int primaryKeyNumber;
    static const std::string tableName;
    static const bool hasPrimaryKey;
    static const std::vector<std::string> primaryKeyName;
    using PrimaryKeyType = std::tuple<int64_t,std::string,int32_t,bool>;//recipe_id,tag_id,slot,input
    PrimaryKeyType getPrimaryKey() const;

    /**
     * @brief constructor
     * @param r One row of records in the SQL query result.
     * @param indexOffset Set the offset to -1 to access all columns by column names,
     * otherwise access all columns by offsets.
     * @note If the SQL is not a style of 'select * from table_name ...' (select all
     * columns by an asterisk), please set the offset to -1.
     */
    explicit RecipeIngredientTag(const drogon::orm::Row &r, const ssize_t indexOffset = 0) noexcept;

    /**
     * @brief constructor
     * @param pJson The json object to construct a new instance.
     */
    explicit RecipeIngredientTag(const Json::Value &pJson) noexcept(false);

    /**
     * @brief constructor
     * @param pJson The json object to construct a new instance.
     * @param pMasqueradingVector The aliases of table columns.
     */
    RecipeIngredientTag(const Json::Value &pJson, const std::vector<std::string> &pMasqueradingVector) noexcept(false);

    RecipeIngredientTag() = default;

    void updateByJson(const Json::Value &pJson) noexcept(false);
    void updateByMasqueradedJson(const Json::Value &pJson,
                                 const std::vector<std::string> &pMasqueradingVector) noexcept(false);
    static bool validateJsonForCreation(const Json::Value &pJson, std::string &err);
    static bool validateMasqueradedJsonForCreation(const Json::Value &,
                                                const std::vector<std::string> &pMasqueradingVector,
                                                    std::string &err);
    static bool validateJsonForUpdate(const Json::Value &pJson, std::string &err);
    static bool validateMasqueradedJsonForUpdate(const Json::Value &,
                                          const std::vector<std::string> &pMasqueradingVector,
                                          std::string &err);
    static bool validJsonOfField(size_t index,
                          const std::string &fieldName,
                          const Json::Value &pJson,
                          std::string &err,
                          bool isForCreation);

    /**  For column recipe_id  */
    ///Get the value of the column recipe_id, returns the default value if the column is null
    const int64_t &getValueOfRecipeId() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<int64_t> &getRecipeId() const noexcept;
    ///Set the value of the column recipe_id
    void setRecipeId(const int64_t &pRecipeId) noexcept;

    /**  For column tag_id  */
    ///Get the value of the column tag_id, returns the default value if the column is null
    const std::string &getValueOfTagId() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<std::string> &getTagId() const noexcept;
    ///Set the value of the column tag_id
    void setTagId(const std::string &pTagId) noexcept;
    void setTagId(std::string &&pTagId) noexcept;

    /**  For column slot  */
    ///Get the value of the column slot, returns the default value if the column is null
    const int32_t &getValueOfSlot() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<int32_t> &getSlot() const noexcept;
    ///Set the value of the column slot
    void setSlot(const int32_t &pSlot) noexcept;

    /**  For column count  */
    ///Get the value of the column count, returns the default value if the column is null
    const int32_t &getValueOfCount() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<int32_t> &getCount() const noexcept;
    ///Set the value of the column count
    void setCount(const int32_t &pCount) noexcept;

    /**  For column input  */
    ///Get the value of the column input, returns the default value if the column is null
    const bool &getValueOfInput() const noexcept;
    ///Return a shared_ptr object pointing to the column const value, or an empty shared_ptr object if the column is null
    const std::shared_ptr<bool> &getInput() const noexcept;
    ///Set the value of the column input
    void setInput(const bool &pInput) noexcept;


    static size_t getColumnNumber() noexcept {  return 5;  }
    static const std::string &getColumnName(size_t index) noexcept(false);

    Json::Value toJson() const;
    Json::Value toMasqueradedJson(const std::vector<std::string> &pMasqueradingVector) const;
    /// Relationship interfaces
  private:
    friend drogon::orm::Mapper<RecipeIngredientTag>;
    friend drogon::orm::BaseBuilder<RecipeIngredientTag, true, true>;
    friend drogon::orm::BaseBuilder<RecipeIngredientTag, true, false>;
    friend drogon::orm::BaseBuilder<RecipeIngredientTag, false, true>;
    friend drogon::orm::BaseBuilder<RecipeIngredientTag, false, false>;
#ifdef __cpp_impl_coroutine
    friend drogon::orm::CoroMapper<RecipeIngredientTag>;
#endif
    static const std::vector<std::string> &insertColumns() noexcept;
    void outputArgs(drogon::orm::internal::SqlBinder &binder) const;
    const std::vector<std::string> updateColumns() const;
    void updateArgs(drogon::orm::internal::SqlBinder &binder) const;
    ///For mysql or sqlite3
    void updateId(const uint64_t id);
    std::shared_ptr<int64_t> recipeId_;
    std::shared_ptr<std::string> tagId_;
    std::shared_ptr<int32_t> slot_;
    std::shared_ptr<int32_t> count_;
    std::shared_ptr<bool> input_;
    struct MetaData
    {
        const std::string colName_;
        const std::string colType_;
        const std::string colDatabaseType_;
        const ssize_t colLength_;
        const bool isAutoVal_;
        const bool isPrimaryKey_;
        const bool notNull_;
    };
    static const std::vector<MetaData> metaData_;
    bool dirtyFlag_[5]={ false };
  public:
    static const std::string &sqlForFindingByPrimaryKey()
    {
        static const std::string sql="select * from " + tableName + " where recipe_id = $1 and tag_id = $2 and slot = $3 and input = $4";
        return sql;
    }

    static const std::string &sqlForDeletingByPrimaryKey()
    {
        static const std::string sql="delete from " + tableName + " where recipe_id = $1 and tag_id = $2 and slot = $3 and input = $4";
        return sql;
    }
    std::string sqlForInserting(bool &needSelection) const
    {
        std::string sql="insert into " + tableName + " (";
        size_t parametersCount = 0;
        needSelection = false;
            sql += "recipe_id,";
            ++parametersCount;
        if(dirtyFlag_[1])
        {
            sql += "tag_id,";
            ++parametersCount;
        }
        if(dirtyFlag_[2])
        {
            sql += "slot,";
            ++parametersCount;
        }
        if(dirtyFlag_[3])
        {
            sql += "count,";
            ++parametersCount;
        }
        if(dirtyFlag_[4])
        {
            sql += "input,";
            ++parametersCount;
        }
        needSelection=true;
        if(parametersCount > 0)
        {
            sql[sql.length()-1]=')';
            sql += " values (";
        }
        else
            sql += ") values (";

        int placeholder=1;
        char placeholderStr[64];
        size_t n=0;
        sql +="default,";
        if(dirtyFlag_[1])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[2])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[3])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(dirtyFlag_[4])
        {
            n = snprintf(placeholderStr,sizeof(placeholderStr),"$%d,",placeholder++);
            sql.append(placeholderStr, n);
        }
        if(parametersCount > 0)
        {
            sql.resize(sql.length() - 1);
        }
        if(needSelection)
        {
            sql.append(") returning *");
        }
        else
        {
            sql.append(1, ')');
        }
        LOG_TRACE << sql;
        return sql;
    }
};
} // namespace postgres
} // namespace drogon_model
