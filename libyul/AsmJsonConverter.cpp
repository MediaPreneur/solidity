/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * @date 2019
 * Converts inline assembly AST to JSON format
 */

#include <libyul/AsmJsonConverter.h>
#include <libyul/AST.h>
#include <libyul/Exceptions.h>
#include <libsolutil/CommonData.h>
#include <libsolutil/UTF8.h>

using namespace std;
using namespace solidity::langutil;
using namespace solidity::yul;

namespace solidity::yul
{

Json::Value AsmJsonConverter::operator()(Block const& _node) const
{
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulBlock");
	ret["statements"] = vectorOfVariantsToJson(_node.statements);
	return ret;
}

Json::Value AsmJsonConverter::operator()(TypedName const& _node) const
{
	yulAssert(!_node.name.empty(), "Invalid variable name.");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulTypedName");
	ret["name"] = _node.name.str();
	ret["type"] = _node.type.str();
	return ret;
}

Json::Value AsmJsonConverter::operator()(Literal const& _node) const
{
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulLiteral");
	switch (_node.kind)
	{
	case LiteralKind::Number:
		yulAssert(
			util::isValidDecimal(_node.value.str()) || util::isValidHex(_node.value.str()),
			"Invalid number literal"
		);
		ret["kind"] = "number";
		break;
	case LiteralKind::Boolean:
		ret["kind"] = "bool";
		break;
	case LiteralKind::String:
		ret["kind"] = "string";
		ret["hexValue"] = util::toHex(util::asBytes(_node.value.str()));
		break;
	}
	ret["type"] = _node.type.str();
	if (util::validateUTF8(_node.value.str()))
		ret["value"] = _node.value.str();
	return ret;
}

Json::Value AsmJsonConverter::operator()(Identifier const& _node) const
{
	yulAssert(!_node.name.empty(), "Invalid identifier");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulIdentifier");
	ret["name"] = _node.name.str();
	return ret;
}

Json::Value AsmJsonConverter::operator()(Assignment const& _node) const
{
	yulAssert(_node.variableNames.size() >= 1, "Invalid assignment syntax");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulAssignment");
	for (auto const& var: _node.variableNames)
		ret["variableNames"].append((*this)(var));
	ret["value"] = _node.value ? std::visit(*this, *_node.value) : Json::nullValue;
	return ret;
}

Json::Value AsmJsonConverter::operator()(FunctionCall const& _node) const
{
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulFunctionCall");
	ret["functionName"] = (*this)(_node.functionName);
	ret["arguments"] = vectorOfVariantsToJson(_node.arguments);
	return ret;
}

Json::Value AsmJsonConverter::operator()(ExpressionStatement const& _node) const
{
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulExpressionStatement");
	ret["expression"] = std::visit(*this, _node.expression);
	return ret;
}

Json::Value AsmJsonConverter::operator()(VariableDeclaration const& _node) const
{
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulVariableDeclaration");
	for (auto const& var: _node.variables)
		ret["variables"].append((*this)(var));

	ret["value"] = _node.value ? std::visit(*this, *_node.value) : Json::nullValue;

	return ret;
}

Json::Value AsmJsonConverter::operator()(FunctionDefinition const& _node) const
{
	yulAssert(!_node.name.empty(), "Invalid function name.");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulFunctionDefinition");
	ret["name"] = _node.name.str();
	for (auto const& var: _node.parameters)
		ret["parameters"].append((*this)(var));
	for (auto const& var: _node.returnVariables)
		ret["returnVariables"].append((*this)(var));
	ret["body"] = (*this)(_node.body);
	return ret;
}

Json::Value AsmJsonConverter::operator()(If const& _node) const
{
	yulAssert(_node.condition, "Invalid if condition.");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulIf");
	ret["condition"] = std::visit(*this, *_node.condition);
	ret["body"] = (*this)(_node.body);
	return ret;
}

Json::Value AsmJsonConverter::operator()(Switch const& _node) const
{
	yulAssert(_node.expression, "Invalid expression pointer.");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulSwitch");
	ret["expression"] = std::visit(*this, *_node.expression);
	for (auto const& var: _node.cases)
		ret["cases"].append((*this)(var));
	return ret;
}

Json::Value AsmJsonConverter::operator()(Case const& _node) const
{
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulCase");
	ret["value"] = _node.value ? (*this)(*_node.value) : "default";
	ret["body"] = (*this)(_node.body);
	return ret;
}

Json::Value AsmJsonConverter::operator()(ForLoop const& _node) const
{
	yulAssert(_node.condition, "Invalid for loop condition.");
	Json::Value ret = createAstNode(debugDataOf(_node).get(), "YulForLoop");
	ret["pre"] = (*this)(_node.pre);
	ret["condition"] = std::visit(*this, *_node.condition);
	ret["post"] = (*this)(_node.post);
	ret["body"] = (*this)(_node.body);
	return ret;
}

Json::Value AsmJsonConverter::operator()(Break const& _node) const
{
	return createAstNode(debugDataOf(_node).get(), "YulBreak");
}

Json::Value AsmJsonConverter::operator()(Continue const& _node) const
{
	return createAstNode(debugDataOf(_node).get(), "YulContinue");
}

Json::Value AsmJsonConverter::operator()(Leave const& _node) const
{
	return createAstNode(debugDataOf(_node).get(), "YulLeave");
}

string AsmJsonConverter::formatLocationWithFileName(SourceLocation const& _location)
{
	return
		to_string(_location.start) +
		":" +
		to_string(_location.length()) +
		":" +
		(_location.sourceName ? *_location.sourceName : "");
}

string AsmJsonConverter::formatLocationWithFileIndex(SourceLocation const& _location, optional<size_t> const& _sourceIndex)
{
	return
		to_string(_location.start) +
		":" +
		to_string(_location.length()) +
		":" +
		(_sourceIndex.has_value() ? to_string(_sourceIndex.value()) : "-1");
}

Json::Value AsmJsonConverter::createAstNode(DebugData const* _debugData, string _nodeType) const
{
	yulAssert(_debugData, "");

	Json::Value ret{Json::objectValue};
	ret["nodeType"] = std::move(_nodeType);

	ret["src"] = formatLocationWithFileIndex(_debugData->nativeLocation, m_sourceIndex);
	if (_debugData->originLocation.isValid())
	{
		if (_debugData->nativeLocation.sourceName == _debugData->originLocation.sourceName)
			ret["origin"] = formatLocationWithFileIndex(_debugData->originLocation, m_sourceIndex);
		else
			ret["origin"] = formatLocationWithFileName(_debugData->originLocation);
	}

	return ret;
}

template <class T>
Json::Value AsmJsonConverter::vectorOfVariantsToJson(vector<T> const& _vec) const
{
	Json::Value ret{Json::arrayValue};
	for (auto const& var: _vec)
		ret.append(std::visit(*this, var));
	return ret;
}

}
