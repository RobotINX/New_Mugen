#include "serialize.h"
#include "util/token.h"
#include "compiler.h"
#include <sstream>

using std::vector;
using std::string;

namespace Mugen{

const char * BOOL_VALUE = "b";
const char * STRING_VALUE = "s";
const char * DOUBLE_VALUE = "d";
const char * LIST_STRING_VALUE = "l";
const char * RANGE_VALUE = "r";
const char * STATE_VALUE = "v";
const char * ATTACK_VALUE = "a";
const char * INTS_VALUE = "i";

Token * serialize(const RuntimeValue & value){
    Token * token = new Token();
    switch (value.getType()){
        case RuntimeValue::Invalid: {
            break;
        }
        case RuntimeValue::Bool: {
            *token << BOOL_VALUE << value.getBoolValue();
            break;
        }
        case RuntimeValue::String: {
            *token << STRING_VALUE << value.getStringValue();
            break;
        }
        case RuntimeValue::Double: {
            *token << DOUBLE_VALUE;
            if (value.getDoubleValue() != 0){
                *token << value.getDoubleValue();
            }
            break;
        }
        case RuntimeValue::ListOfString: {
            *token << LIST_STRING_VALUE;
            for (vector<string>::const_iterator it = value.strings_value.begin(); it != value.strings_value.end(); it++){
                *token << *it;
            }
            break;
        }
        case RuntimeValue::RangeType: {
            *token << RANGE_VALUE << value.range.low << value.range.high;
            break;
        }
        case RuntimeValue::StateType: {
            *token << STATE_VALUE <<
                value.attribute.standing <<
                value.attribute.crouching <<
                value.attribute.lying <<
                value.attribute.aerial;
            break;
        }
        case RuntimeValue::AttackAttribute: {
            *token << ATTACK_VALUE; 
            for (vector<AttackType::Attribute>::const_iterator it = value.attackAttributes.begin(); it != value.attackAttributes.end(); it++){
                *token << *it;
            }
            break;
        }
        case RuntimeValue::ListOfInt: {
            *token << INTS_VALUE;
            for (vector<int>::const_iterator it = value.ints_value.begin(); it != value.ints_value.end(); it++){
                *token << *it;
            }

            break;
        }
    }

    return token;
}

Token * serialize(const AttackType::Attribute data){
    Token * token = new Token();
    *token << data;
    return token;
}

Token * serialize(const AttackType::Animation data){
    std::ostringstream out;
    out << data;
    return new Token(out.str());
}

Token * serialize(const AttackType::Ground data){
    std::ostringstream out;
    out << data;
    return new Token(out.str());
}

Token * serialize(const TransType data){
    std::ostringstream out;
    out << data;
    return new Token(out.str());
}

Token * serialize(const CharacterId & data){
    std::ostringstream out;
    out << data.intValue();
    return new Token(out.str());
}

Token * serialize(const std::vector<CharacterId> & data){
    Token * token = new Token();
    *token << "ids";
    for (vector<CharacterId>::const_iterator it = data.begin(); it != data.end(); it++){
        *token << it->intValue();
    }
    return token;
}

Token * serialize(const std::string & data){
    return new Token(data);
}

}