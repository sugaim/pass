// ConsoleApplication1.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <iostream>
#include <string_view>
#include <memory>
#include <type_traits>
#include <optional>
#include <stdexcept>

namespace sample::detail {
// -----------------------------------------------------------------------------
//  is_spec_of
// -----------------------------------------------------------------------------
    template <typename T, template <typename...> typename TT>
    struct is_spec_of : std::false_type {};

    template <template <typename ...> typename TT, typename ...Ts>
    struct is_spec_of<TT<Ts...>, TT> : std::true_type {};

    template <typename T, template <typename...> typename TT>
    struct is_spec_of<const T, TT> : is_spec_of<T, TT> {};

// -----------------------------------------------------------------------------
//  rep_str
// -----------------------------------------------------------------------------
    std::string rep_str(std::string_view view, int n)
    {
        std::string result;
        result.resize(view.size() * n);
        for (int i=0; i<n; ++i) {
            result.append(view);
        }
        return result;
    }

} // namespace sample::detail

namespace sample {
// -----------------------------------------------------------------------------
//  maybe
// -----------------------------------------------------------------------------
    template <typename T>
    class maybe {
    private:
        static_assert(!std::is_reference_v<T>);
        using this_type = maybe;

    public:
    // -------------------------------------------------------------------------
    //  ctors
    //
        constexpr maybe() = default;
        constexpr maybe(const this_type&) = default;
        constexpr maybe(this_type&&) = default;

        constexpr maybe(std::nullopt_t)
            : internal_(), has_value_(false)
        {
        }

        constexpr explicit maybe(const T& value)
            : internal_(value), has_value_(true)
        {
        }

        constexpr explicit maybe(T&& value)
            : internal_(std::move(value)), has_value_(true)
        {
        }

        constexpr this_type& operator =(const this_type&) & = default;
        constexpr this_type& operator =(this_type&&) & = default;

    // -------------------------------------------------------------------------
    //  access
    //
        constexpr bool has_value() const noexcept { return has_value_; }

        constexpr const T& value() const &
        { 
            if (this->has_value()) {
                return this->internal_;
            }
            else {
                throw std::logic_error("invalid access]");
            }
        }

        constexpr T&& value() &&
        { 
            if (this->has_value()) {
                return std::move(this->internal_);
            }
            else {
                throw std::logic_error("invalid access]");
            }
        }

    private:
        T internal_;
        bool has_value_ = false;

    }; // class maybe

// -----------------------------------------------------------------------------
//  animal
// -----------------------------------------------------------------------------
    class animal {
    public:
        virtual ~animal() = default;
        virtual void make_sound(int n) const = 0;
        virtual std::unique_ptr<animal> clone() const = 0;
        
    }; // class animal

// -----------------------------------------------------------------------------
//  dog, cat
// -----------------------------------------------------------------------------
    class dog : public animal {
    public:
        void make_sound(int n) const override { std::cout << detail::rep_str("woof", n) << std::endl; }
        std::unique_ptr<animal> clone() const override { return std::make_unique<dog>(); }
        void paw() const { std::cout << "woof?" << std::endl; }
        
    }; // class dog

    class cat : public animal {
    public:
        void make_sound(int n) const override { std::cout << detail::rep_str("meow", n) << std::endl; }
        std::unique_ptr<animal> clone() const override { return std::make_unique<cat>(); }
        void zzz() const { std::cout << "zzz" << std::endl; } 

    }; // class dog

// -----------------------------------------------------------------------------
//  parrot
// -----------------------------------------------------------------------------
    class parrot : public animal {
    private:
        using this_type = parrot;

        parrot(std::unique_ptr<animal>&& anm)
            : internal_(std::move(anm))
        {
        }

        parrot(const animal& anm)
            : parrot(anm.clone())
        {
        }

    public:
    // -------------------------------------------------------------------------
    //  ctors
    //
        parrot() noexcept = default;
        parrot(const this_type& other)
            : internal_(other.internal_->clone())
        {            
        }

        parrot(this_type&&) noexcept = default;

        ~parrot() override = default;

        static parrot mimic(const animal& anm) { return parrot(anm); }

        this_type& operator =(const this_type&) & = default;
        this_type& operator =(this_type&&) & = default;

    // -------------------------------------------------------------------------
    //  animal behaviors
    //
        void make_sound(int n) const override
        {
            if (internal_) {
                internal_->make_sound(n);
            }
            else {
                std::cout << detail::rep_str("...?", n) << std::endl;
            }
        }

        std::unique_ptr<animal> clone() const override
        {
            if (internal_) {
                return std::unique_ptr<animal>(new parrot(internal_->clone()));
            }
            else {
                return std::make_unique<parrot>();
            }
        }

    // -------------------------------------------------------------------------
    //  cast
    //
        template <typename T>
        operator std::shared_ptr<T>() const
        { return std::dynamic_pointer_cast<T>(internal_); }

        template <typename T>
        operator maybe<T>() const
        {
            std::shared_ptr<T> ptr = *this;
            return ptr ? maybe<T>(*ptr) : maybe<T>(std::nullopt);
        }

        template <
            typename T,
            std::enable_if_t<
                std::is_base_of_v<animal, T> &&
                !detail::is_spec_of<T, std::shared_ptr>::value &&
                !detail::is_spec_of<T, maybe>::value,
                std::nullptr_t
            > = nullptr
        >
        operator T() const
        {
            if (std::shared_ptr<T> ptr = *this; ptr) {
                return static_cast<T>(*ptr);
            } else {
                throw std::logic_error("invalid cast");
            }
        }

    private:
        std::shared_ptr<animal> internal_;

    }; // class parrot

} // namespace sample



int main()
{
    //
    // preparation
    //
    sample::dog pochi;
    pochi.make_sound(1);

    sample::cat tama;
    tama.make_sound(1);

    auto prt = sample::parrot::mimic(pochi);
    prt.make_sound(2);


    //
    // cast to externally specified type
    //
    sample::dog dg3 = prt;
    dg3.paw();

    //sample::cat ct = prt; // <- exception occurs due to invalid cast
    //ct.zzz();


    //
    // cast to externally specified type with check
    //
    if (sample::maybe<sample::dog> dg = prt; dg.has_value())
    {
        dg.value().make_sound(3);
        dg.value().paw();
    }
    if (sample::maybe<sample::cat> ct = prt; ct.has_value())
    {
        ct.value().make_sound(3);
        ct.value().zzz();
    }

    //
    // cast to externally specified type as ptr with check
    //
    if (std::shared_ptr<sample::dog> dg = prt; dg)
    {
        dg->make_sound(4);
        dg->paw();
    }
    if (std::shared_ptr<sample::cat> ct = prt; ct)
    {
        ct->make_sound(4);
        ct->zzz();
    }
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
