
#include <iostream>
#include <celengine/constellation.h>
#include "name.h"
#include <celutil/utf8.h>

uint32_t NameDatabase::getNameCount() const
{
    return nameIndex.size();
}

void NameDatabase::add(const uint32_t catalogNumber, const utf8_string& name)
{
    if (name.length() != 0)
    {
        utf8_string fname = ReplaceGreekLetterAbbr(name.cpp_str());
#ifdef DEBUG
        uint32_t tmp;
        if ((tmp = getCatalogNumberByName(name)) != InvalidCatalogNumber)
            DPRINTF(2,"Duplicated name '%s' on object with catalog numbers: %d and %d\n", fname.c_str(), tmp, catalogNumber);
#endif
        // Add the new name
        //nameIndex.insert(NameIndex::value_type(name, catalogNumber));

        nameIndex[fname]   = catalogNumber;
        numberIndex.insert(NumberIndex::value_type(catalogNumber, fname));
    }
}
void NameDatabase::erase(const uint32_t catalogNumber)
{
    numberIndex.erase(catalogNumber);
}

uint32_t NameDatabase::getCatalogNumberByName(const utf8_string& name) const
{
    NameIndex::const_iterator iter = nameIndex.find(name);

    if (iter == nameIndex.end())
        return InvalidCatalogNumber;
    else
        return iter->second;
}

// Return the first name matching the catalog number or end()
// if there are no matching names.  The first name *should* be the
// proper name of the OBJ, if one exists. This requires the
// OBJ name database file to have the proper names listed before
// other designations.  Also, the STL implementation must
// preserve this order when inserting the names into the multimap
// (not certain whether or not this behavior is in the STL spec.
// but it works on the implementations I've tried so far.)
std::string NameDatabase::getNameByCatalogNumber(const uint32_t catalogNumber) const
{
    if (catalogNumber == InvalidCatalogNumber)
        return "";

    NumberIndex::const_iterator iter = numberIndex.lower_bound(catalogNumber);

    if (iter != numberIndex.end() && iter->first == catalogNumber)
        return iter->second.cpp_str();

    return "";
}


// Return the first name matching the catalog number or end()
// if there are no matching names.  The first name *should* be the
// proper name of the OBJ, if one exists. This requires the
// OBJ name database file to have the proper names listed before
// other designations.  Also, the STL implementation must
// preserve this order when inserting the names into the multimap
// (not certain whether or not this behavior is in the STL spec.
// but it works on the implementations I've tried so far.)
NameDatabase::NumberIndex::const_iterator NameDatabase::getFirstNameIter(const uint32_t catalogNumber) const
{
    NumberIndex::const_iterator iter = numberIndex.lower_bound(catalogNumber);

    if (iter == numberIndex.end() || iter->first != catalogNumber)
        return getFinalNameIter();
    else
        return iter;
}

NameDatabase::NumberIndex::const_iterator NameDatabase::getFinalNameIter() const
{
    return numberIndex.end();
}

completion_t NameDatabase::getCompletion(const utf8_string& name, bool greek) const
{
    if (greek)
    {
        auto compList = getGreekCompletion(name.cpp_str());
        compList.push_back(name.cpp_str());
        return getCompletion(compList);
    }

    completion_t completion;

    for (NameIndex::const_iterator iter = nameIndex.begin(); iter != nameIndex.end(); ++iter)
    {
        if (isSubstring(iter->first, name, true))
        {
            completion.push_back(iter->first);
        }
    }
    return completion;
}

completion_t NameDatabase::getCompletion(const std::vector<std::string> &list) const
{
    completion_t completion;
    for (const auto &n : list)
        for (const auto &nn : getCompletion(n, false))
            completion.push_back(nn);

    return completion;
}

//
// Author: Toti <root@totibox>, (C) 2005
//


uint32_t NameDatabase::findCatalogNumberByName(const utf8_string& name) const
{
    uint32_t catalogNumber = getCatalogNumberByName(name);
    if (catalogNumber != InvalidCatalogNumber)
        return catalogNumber;

    utf8_string priName   = name;
    utf8_string altName;

    // See if the name is a Bayer or Flamsteed designation
    utf8_string::size_type pos  = name.find(' ');
    if (pos != 0 && pos != utf8_string::npos && pos < name.length() - 1)
    {
        utf8_string prefix(name, 0, pos);
        utf8_string conName(name, pos + 1, utf8_string::npos);
        Constellation* con  = Constellation::getConstellation(conName.cpp_str());
        if (con != nullptr)
        {
            char digit  = ' ';
            int len     = prefix.length();

            // If the first character of the prefix is a letter
            // and the last character is a digit, we may have
            // something like 'Alpha2 Cen' . . . Extract the digit
            // before trying to match a Greek letter.
            if (len > 2 && isalpha(prefix[0]) && isdigit(prefix[len - 1]))
            {
                --len;
                digit   = prefix[len];
            }

            // We have a valid constellation as the last part
            // of the name.  Next, we see if the first part of
            // the name is a greek letter.
            const utf8_string& letter = Greek::canonicalAbbreviation(utf8_string(prefix, 0, len).cpp_str());
            if (letter != "")
            {
                // Matched . . . this is a Bayer designation
                if (digit == ' ')
                {
                    priName  = letter + ' ' + con->getAbbreviation();
                    // If 'let con' doesn't match, try using
                    // 'let1 con' instead.
                    altName  = letter + '1' + ' ' + con->getAbbreviation();
                }
                else
                {
                    priName  = letter + digit + ' ' + con->getAbbreviation();
                }
            }
            else
            {
                // Something other than a Bayer designation
                priName  = prefix + ' ' + con->getAbbreviation();
            }
        }
    }

    catalogNumber = getCatalogNumberByName(priName);
    if (catalogNumber != InvalidCatalogNumber)
        return catalogNumber;

    priName        += " A";  // try by appending an A
    catalogNumber   = getCatalogNumberByName(priName);
    if (catalogNumber != InvalidCatalogNumber)
        return catalogNumber;

    // If the first search failed, try using the alternate name
    if (altName.length() != 0)
    {
        catalogNumber   = getCatalogNumberByName(altName);
        if (catalogNumber == InvalidCatalogNumber)
        {
            altName        += " A";
            catalogNumber   = getCatalogNumberByName(altName);
        }   // Intentional fallthrough.
    }

    return catalogNumber;
}


bool NameDatabase::loadNames(std::istream& in)
{
    bool failed = false;
    utf8_string s;

    while (!failed)
    {
        uint32_t catalogNumber = InvalidCatalogNumber;

        in >> catalogNumber;
        if (in.eof())
            break;
        if (in.bad())
        {
            failed = true;
            break;
        }

        // in.get(); // skip a space (or colon);

        std::string name;
        getline(in, name);
        if (in.bad())
        {
            failed = true;
            break;
        }

        // Iterate through the string for names delimited
        // by ':', and insert them into the star database. Note that
        // db->add() will skip empty names.
        utf8_string::size_type startPos = 0;
        while (startPos != utf8_string::npos)
        {
            ++startPos;
            utf8_string::size_type next = name.find(':', startPos);
            utf8_string::size_type length = utf8_string::npos;

            if (next != utf8_string::npos)
                length = next - startPos;

            add(catalogNumber, name.substr(startPos, length));
            startPos = next;
        }
    }

    return !failed;
}
