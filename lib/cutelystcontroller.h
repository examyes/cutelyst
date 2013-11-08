/*
 * Copyright (C) 2013 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CUTELYSTCONTROLLER_H
#define CUTELYSTCONTROLLER_H

#include <QObject>

class CutelystContext;
class CutelystController : public QObject
{
    Q_OBJECT
    /**
      * Use Q_CLASSINFO to give hints about methods
      * build like methodName_option
      * Where option is one of the following:
      *
      * Path - An ending path relative to the class info Namespace
      * for example:
      * "" - /namespace/controlername (used for the index)
      * "foo" - /namespace/controlername/foo
      * "/bar" - /namespace/bar
      *
      * Chained - Sets the name of this part of the chain. If it
      * is specified without arguments, it takes the name of
      * the action as default.
      *
      * PathPart - The part of the chained path
      *
      * Args - In the case of more than 9 parameters, to build
      * the path set the needed number here, where an empty string
      * means unlimited arguments.
      *
      * CaptureArgs - In the case of more than 9 parameters, to
      * be captured the path set the needed number here, where -1
      * means unlimited arguments.
      */
public:
    /**
     * @brief Global - Alias to Path="/methodname" which sets the
     * method relative to your root
     * Always add it to the end of the argument list of the methods
     */
    typedef int Global;

    /**
     * @brief Local - Alias to Path="methodname"
     * Always add it to the end of the argument list of the methods
     */
    typedef int Local;

    /**
     * @brief Args - When used with "Path"  indicates the number of
     * arguments expected in the path.
     * However if no Args value is set, assumed to 'slurp' all
     * remaining path parts under this namespace.
     * This is ignored if Q_CLASSINFO has defined it before
     * Always add it to the end of the argument list of the methods.
     */
    typedef int Args;

    Q_INVOKABLE explicit CutelystController(QObject *parent = 0);
    ~CutelystController();

    /**
     * This specifies the internal namespace the controller should
     * be bound to.
     * By default the controller is bound to the URI version of the
     * controller name. For instance a controller named
     * 'MyFooBar' will be bound to 'my/foo/bar'.
     * The default Root controller is an example of setting
     * namespace to '' (the null string).
     */
    QString ns() const;

    CutelystContext* c() const;
    void setContext(CutelystContext *c);

protected:
    virtual void dispatchBegin();
    virtual void dispatchAuto();
    virtual void dispatchAction();
    virtual void dispatchEnd();

private:
    Q_INVOKABLE void _DISPATCH();
    Q_INVOKABLE bool _BEGIN();
    Q_INVOKABLE bool _AUTO();
    Q_INVOKABLE bool _ACTION();
    Q_INVOKABLE bool _END();

    CutelystContext *m_c;
};

Q_DECLARE_METATYPE(CutelystController*)

#endif // CUTELYSTCONTROLLER_H
